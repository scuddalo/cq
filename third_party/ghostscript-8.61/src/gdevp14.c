/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/
/* $Id: gdevp14.c 8395 2007-11-21 06:10:53Z henrys $	*/
/* Compositing devices for implementing	PDF 1.4	imaging	model */

#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gscdefs.h"
#include "gxdevice.h"
#include "gsdevice.h"
#include "gsstruct.h"
#include "gxistate.h"
#include "gxdcolor.h"
#include "gxiparam.h"
#include "gstparam.h"
#include "gxblend.h"
#include "gxtext.h"
#include "gsdfilt.h"
#include "gsimage.h"
#include "gsrect.h"
#include "gzstate.h"
#include "gdevdevn.h"
#include "gdevp14.h"
#include "gsovrc.h"
#include "gxcmap.h"
#include "gscolor1.h"
#include "gstrans.h"
#include "gsutil.h"
#include "gxcldev.h"
#include "gxdcconv.h"

/*
 * We chose the blending color space based upon the process color model of the
 * output device.  For gray, RGB, CMYK, or CMYK+spot devices, the choice is
 * usually simple.  For other devices or if the user is doing custom color
 * processing then the user may want to control this choice.
 */
#define AUTO_CUSTOM_BLENDING 0
#define ALWAYS_USE_CUSTOM_BLENDING 1
#define DO_NOT_USE_CUSTOM_BLENDING 2

#define CUSTOM_BLENDING_MODE AUTO_CUSTOM_BLENDING

/* #define DUMP_TO_PNG */

#ifdef DUMP_TO_PNG
#include "png_.h"
#endif

# define INCR(v) DO_NOTHING

/* Prototypes to quiet gcc warnings */
void pdf14_cmyk_cs_to_cmyk_cm(gx_device *, frac, frac, frac, frac, frac *);
int gs_pdf14_device_push(gs_memory_t *, gs_imager_state *, gx_device **,
			gx_device *, const gs_pdf14trans_t *);

/* Buffer stack	data structure */

#define	PDF14_MAX_PLANES GX_DEVICE_COLOR_MAX_COMPONENTS

/* GC procedures for buffer stack */

static
ENUM_PTRS_WITH(pdf14_buf_enum_ptrs, pdf14_buf *buf)
    return 0;
    case 0: return ENUM_OBJ(buf->saved);
    case 1: return ENUM_OBJ(buf->data);
    case 2: return ENUM_OBJ(buf->transfer_fn);
ENUM_PTRS_END

static
RELOC_PTRS_WITH(pdf14_buf_reloc_ptrs, pdf14_buf	*buf)
{
    RELOC_VAR(buf->saved);
    RELOC_VAR(buf->data);
    RELOC_VAR(buf->transfer_fn);
}
RELOC_PTRS_END

gs_private_st_composite(st_pdf14_buf, pdf14_buf, "pdf14_buf",
			pdf14_buf_enum_ptrs, pdf14_buf_reloc_ptrs);

gs_private_st_ptrs2(st_pdf14_ctx, pdf14_ctx, "pdf14_ctx",
		    pdf14_ctx_enum_ptrs, pdf14_ctx_reloc_ptrs,
		    stack, maskbuf);

/*
 * Unpack a device color.  This routine is similar to the device's
 * decode_color procedure except for two things.  The procedure produces 1
 * byte values instead of gx_color_values (2 bytes) and the output values
 * are inverted for subtractive color spaces (like CMYK).  A separate
 * procedure is used instead of the decode_color to minimize execution time.
 */
static void
pdf14_unpack_additive(int num_comp, gx_color_index color,
			       	pdf14_device * p14dev, byte * out)
{
    int i;

    for (i = num_comp - 1; i >= 0; i--) {
	out[i] = (byte)(color & 0xff);
	color >>= 8;
    }
}

/*
 * Unpack a device color.  This routine is similar to the device's
 * decode_color procedure except for two things.  The procedure produces 1
 * byte values instead of gx_color_values (2 bytes) and the output values
 * are inverted for subtractive color spaces (like CMYK).  A separate
 * procedure is used instead of the decode_color to minimize execution time.
 */
static void
pdf14_unpack_subtractive(int num_comp, gx_color_index color,
			       	pdf14_device * p14dev, byte * out)
{
    int i;

    for (i = num_comp - 1; i >= 0; i--) {
	out[i] = 0xff - (byte)(color & 0xff);
	color >>= 8;
    }
}

/*
 * Unpack a 'compressed' CMYK color index.  The color index value is unpacked
 * into a set of 8 bit values.  For more information about 'compressed' color
 * index values see the comments before the devn_encode_compressed_color routine.
 *
 * Note: For simplicity of coding the calling routines, this routine will also
 * handle 'uncompressed' color index values.
 */
static void
pdf14_unpack_compressed(int num_comp, gx_color_index color,
			       	pdf14_device * p14dev, byte * out)
{
    int comp_num;

    if (p14dev->devn_params.compressed_color_list == NULL) {
        /*
         * For 'uncompressed' data we simply have to unpack the gx_color_index
	 * value directly.
         */
        for (comp_num = num_comp - 1; comp_num >= 0; comp_num--) {
	    out[comp_num] = 0xff - (byte)(color & 0xff);
	    color >>= 8;
        }
    }
    else {
        int factor, bit_count, bit_mask;
        comp_bit_map_list_t * pbitmap;
        gx_color_value solid_color = 0xff;

    	pbitmap = find_bit_map(color,
		       	p14dev->devn_params.compressed_color_list);
	bit_count = num_comp_bits[pbitmap->num_non_solid_comp];
	bit_mask = (1 << bit_count) - 1;
	factor = comp_bit_factor[pbitmap->num_non_solid_comp];
	if (pbitmap->solid_not_100) {
	    solid_color = 0xff - ((factor * ((int)color & bit_mask)) >> 16);
	    color >>= bit_count;
	}
        for (comp_num = 0; comp_num < num_comp; comp_num++) {
	    if (colorant_present(pbitmap, colorants, comp_num)) {
		if (colorant_present(pbitmap, solid_colorants, comp_num))
       		    *out++ = (byte)solid_color;
		else {
            	    *out++ = 0xff - ((factor * ((int)color & bit_mask)) >> 16);
	    	    color >>= bit_count;
		}
	    }
	    else
            	*out++ = 0xff;
        }
    }
}

/*
 * Unpack a device color.  This routine is used for devices in which we do
 * not know the details of the process color model.  In this case we use
 * the device's decode_color procedure.
 */
static void
pdf14_unpack_custom(int num_comp, gx_color_index color,
			       	pdf14_device * p14dev, byte * out)
{
    int i;
    gx_device * tdev = p14dev->target;
    gx_color_value cm_values[GX_DEVICE_COLOR_MAX_COMPONENTS];

    dev_proc(tdev, decode_color)(tdev, color, cm_values);
    for (i = 0; i < num_comp; i++)
	out[i] = 0xff - gx_color_value_to_byte(cm_values[i]);
}

/* ------ The device descriptors ------	*/

/*
 * Default X and Y resolution.
 */
#define	X_DPI 72
#define	Y_DPI 72

static	int pdf14_open(gx_device * pdev);
static	dev_proc_close_device(pdf14_close);
static	int pdf14_output_page(gx_device	* pdev,	int num_copies,	int flush);
static	dev_proc_put_params(pdf14_put_params);
static	dev_proc_get_color_comp_index(pdf14_cmykspot_get_color_comp_index);
static	dev_proc_get_color_mapping_procs(pdf14_cmykspot_get_color_mapping_procs);
static	dev_proc_encode_color(pdf14_encode_color);
static	dev_proc_encode_color(pdf14_compressed_encode_color);
static	dev_proc_decode_color(pdf14_decode_color);
static	dev_proc_decode_color(pdf14_compressed_decode_color);
static	dev_proc_fill_rectangle(pdf14_fill_rectangle);
static	dev_proc_fill_rectangle(pdf14_mark_fill_rectangle);
static	dev_proc_fill_rectangle(pdf14_mark_fill_rectangle_ko_simple);
static	dev_proc_fill_path(pdf14_fill_path);
static dev_proc_copy_mono(pdf14_copy_mono);
static	dev_proc_stroke_path(pdf14_stroke_path);
static	dev_proc_begin_typed_image(pdf14_begin_typed_image);
static	dev_proc_text_begin(pdf14_text_begin);
static	dev_proc_create_compositor(pdf14_create_compositor);
static	dev_proc_create_compositor(pdf14_forward_create_compositor);
static	dev_proc_begin_transparency_group(pdf14_begin_transparency_group);
static	dev_proc_end_transparency_group(pdf14_end_transparency_group);
static	dev_proc_begin_transparency_mask(pdf14_begin_transparency_mask);
static	dev_proc_end_transparency_mask(pdf14_end_transparency_mask);
static int pdf14_clist_get_param_compressed_color_list(pdf14_device * p14dev);

static	const gx_color_map_procs *
    pdf14_get_cmap_procs(const gs_imager_state *, const gx_device *);

#define	XSIZE (int)(8.5	* X_DPI)	/* 8.5 x 11 inch page, by default */
#define	YSIZE (int)(11 * Y_DPI)

/* 24-bit color. */

#define	pdf14_dev_procs(get_color_mapping_procs, get_color_comp_index, encode_color, decode_color) \
{\
	pdf14_open,			/* open */\
	NULL,				/* get_initial_matrix */\
	NULL,				/* sync_output */\
	pdf14_output_page,		/* output_page */\
	pdf14_close,			/* close */\
	encode_color,			/* rgb_map_rgb_color */\
	decode_color,			/* gx_default_rgb_map_color_rgb */\
	pdf14_fill_rectangle,		/* fill_rectangle */\
	NULL,				/* tile_rectangle */\
	pdf14_copy_mono,		/* copy_mono */\
	NULL,				/* copy_color */\
	NULL,				/* draw_line */\
	NULL,				/* get_bits */\
	NULL,				/* get_params */\
	pdf14_put_params,		/* put_params */\
	NULL,				/* map_cmyk_color */\
	NULL,				/* get_xfont_procs */\
	NULL,				/* get_xfont_device */\
	NULL,				/* map_rgb_alpha_color */\
	NULL,				/* get_page_device */\
	NULL,				/* get_alpha_bits */\
	NULL,				/* copy_alpha */\
	NULL,				/* get_band */\
	NULL,				/* copy_rop */\
	pdf14_fill_path,		/* fill_path */\
	pdf14_stroke_path,		/* stroke_path */\
	NULL,				/* fill_mask */\
	NULL,				/* fill_trapezoid */\
	NULL,				/* fill_parallelogram */\
	NULL,				/* fill_triangle */\
	NULL,				/* draw_thin_line */\
	NULL,				/* begin_image */\
	NULL,				/* image_data */\
	NULL,				/* end_image */\
	NULL,				/* strip_tile_rectangle */\
	NULL,				/* strip_copy_rop, */\
	NULL,				/* get_clipping_box */\
	pdf14_begin_typed_image,	/* begin_typed_image */\
	NULL,				/* get_bits_rectangle */\
	NULL,				/* map_color_rgb_alpha */\
	pdf14_create_compositor,	/* create_compositor */\
	NULL,				/* get_hardware_params */\
	pdf14_text_begin,		/* text_begin */\
	NULL,				/* finish_copydevice */\
	pdf14_begin_transparency_group,\
	pdf14_end_transparency_group,\
	pdf14_begin_transparency_mask,\
	pdf14_end_transparency_mask,\
	NULL,				/* discard_transparency_layer */\
	get_color_mapping_procs,	/* get_color_mapping_procs */\
	get_color_comp_index,		/* get_color_comp_index */\
	encode_color,			/* encode_color */\
	decode_color,			/* decode_color */\
	NULL,				/* pattern_manage */\
	NULL,				/* fill_rectangle_hl_color */\
	NULL,				/* include_color_space */\
	NULL,				/* fill_linear_color_scanline */\
	NULL,				/* fill_linear_color_trapezoid */\
	NULL,				/* fill_linear_color_triangle */\
	gx_forward_update_spot_equivalent_colors	/* update spot */\
}

static	const gx_device_procs pdf14_Gray_procs =
	pdf14_dev_procs(gx_default_DevGray_get_color_mapping_procs,
			gx_default_DevGray_get_color_comp_index,
		       	pdf14_encode_color, pdf14_decode_color);

static	const gx_device_procs pdf14_RGB_procs =
	pdf14_dev_procs(gx_default_DevRGB_get_color_mapping_procs,
			gx_default_DevRGB_get_color_comp_index,
		       	pdf14_encode_color, pdf14_decode_color);

static	const gx_device_procs pdf14_CMYK_procs =
	pdf14_dev_procs(gx_default_DevCMYK_get_color_mapping_procs,
			gx_default_DevCMYK_get_color_comp_index,
		       	pdf14_encode_color, pdf14_decode_color);

static	const gx_device_procs pdf14_CMYKspot_procs =
	pdf14_dev_procs(pdf14_cmykspot_get_color_mapping_procs,
			pdf14_cmykspot_get_color_comp_index,
		       	pdf14_compressed_encode_color,
		       	pdf14_compressed_decode_color);

static	const gx_device_procs pdf14_custom_procs =
	pdf14_dev_procs(gx_forward_get_color_mapping_procs,
			gx_forward_get_color_comp_index,
		       	gx_forward_encode_color,
		       	gx_forward_decode_color);

gs_private_st_composite_use_final(st_pdf14_device, pdf14_device, "pdf14_device",
				  pdf14_device_enum_ptrs, pdf14_device_reloc_ptrs,
			  gx_device_finalize);

static int pdf14_put_image(gx_device * dev, gs_imager_state * pis,
	       						gx_device * target);
static int pdf14_cmykspot_put_image(gx_device * dev, gs_imager_state * pis,
	       						gx_device * target);
static int pdf14_custom_put_image(gx_device * dev, gs_imager_state * pis,
	       						gx_device * target);

static const pdf14_procs_t gray_pdf14_procs = {
    pdf14_unpack_additive,
    pdf14_put_image
};

static const pdf14_procs_t rgb_pdf14_procs = {
    pdf14_unpack_additive,
    pdf14_put_image
};

static const pdf14_procs_t cmyk_pdf14_procs = {
    pdf14_unpack_subtractive,
    pdf14_put_image
};

static const pdf14_procs_t cmykspot_pdf14_procs = {
    pdf14_unpack_compressed,
    pdf14_cmykspot_put_image
};

static const pdf14_procs_t custom_pdf14_procs = {
    pdf14_unpack_custom,
    pdf14_custom_put_image
};

static const pdf14_nonseparable_blending_procs_t gray_blending_procs = {
    art_blend_luminosity_rgb_8,
    art_blend_saturation_rgb_8
};

static const pdf14_nonseparable_blending_procs_t rgb_blending_procs = {
    art_blend_luminosity_rgb_8,
    art_blend_saturation_rgb_8
};

static const pdf14_nonseparable_blending_procs_t cmyk_blending_procs = {
    art_blend_luminosity_cmyk_8,
    art_blend_saturation_cmyk_8
};

static const pdf14_nonseparable_blending_procs_t custom_blending_procs = {
    art_blend_luminosity_custom_8,
    art_blend_saturation_custom_8
};

const pdf14_device gs_pdf14_Gray_device	= {
    std_device_color_stype_body(pdf14_device, &pdf14_Gray_procs, "pdf14gray",
				&st_pdf14_device,
				XSIZE, YSIZE, X_DPI, Y_DPI, 8, 255, 256),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &gray_pdf14_procs,
    &gray_blending_procs
};

const pdf14_device gs_pdf14_RGB_device = {
    std_device_color_stype_body(pdf14_device, &pdf14_RGB_procs, "pdf14RGB",
				&st_pdf14_device,
				XSIZE, YSIZE, X_DPI, Y_DPI, 24, 255, 256),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &rgb_pdf14_procs,
    &rgb_blending_procs
};

const pdf14_device gs_pdf14_CMYK_device	= {
    std_device_std_color_full_body_type(pdf14_device, &pdf14_CMYK_procs,
	    "PDF14cmyk", &st_pdf14_device, XSIZE, YSIZE, X_DPI, Y_DPI, 32,
	    0, 0, 0, 0, 0, 0),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &cmyk_pdf14_procs,
    &cmyk_blending_procs
};

const pdf14_device gs_pdf14_CMYKspot_device	= {
    std_device_part1_(pdf14_device, &pdf14_CMYKspot_procs, "PDF14cmykspot", &st_pdf14_device, open_init_closed),
    dci_values(GX_DEVICE_COLOR_MAX_COMPONENTS,64,255,255,256,256),
    std_device_part2_(XSIZE, YSIZE, X_DPI, Y_DPI),
    offset_margin_values(0, 0, 0, 0, 0, 0),
    std_device_part3_(),
    { 0 },			/* Procs */
    NULL,			/* target */
    /* DeviceN parameters */
    { 8,			/* Not used - Bits per color */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    &cmykspot_pdf14_procs,
    &cmyk_blending_procs
};

/*
 * The 'custom' PDF 1.4 compositor device is for working with those devices
 * which support spot colors but do not have a CMYK process color model.
 *
 * This causes some problems with the Hue, Saturation, Color, and Luminosity
 * blending modes.  These blending modes are 'non separable' and depend upon
 * knowing the details of the blending color space.  However we use the
 * process color model of the output device for our blending color space.
 * With an unknown process color model, we have to fall back to some 'guesses'
 * about how to treat these blending modes.
 */
const pdf14_device gs_pdf14_custom_device = {
    std_device_part1_(pdf14_device, &pdf14_custom_procs, "PDF14custom", &st_pdf14_device, open_init_closed),
    dci_values(GX_DEVICE_COLOR_MAX_COMPONENTS,64,255,255,256,256),
    std_device_part2_(XSIZE, YSIZE, X_DPI, Y_DPI),
    offset_margin_values(0, 0, 0, 0, 0, 0),
    std_device_part3_(),
    { 0 },			/* Procs */
    NULL,			/* target */
    /* DeviceN parameters */
    { 8,			/* Not used - Bits per color */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    &custom_pdf14_procs,
    &custom_blending_procs
};

/* GC procedures */
static	
ENUM_PTRS_WITH(pdf14_device_enum_ptrs, pdf14_device *pdev)
{
    index -= 3;
    if (index < pdev->devn_params.separations.num_separations)
	ENUM_RETURN(pdev->devn_params.separations.names[index].data);
    index -= pdev->devn_params.separations.num_separations;
    if (index < pdev->devn_params.pdf14_separations.num_separations)
	ENUM_RETURN(pdev->devn_params.pdf14_separations.names[index].data);
    return 0;
}
case 0:	return ENUM_OBJ(pdev->ctx);
case 1:	ENUM_RETURN(gx_device_enum_ptr(pdev->target));
case 2: ENUM_RETURN(pdev->devn_params.compressed_color_list);
ENUM_PTRS_END

static	RELOC_PTRS_WITH(pdf14_device_reloc_ptrs, pdf14_device *pdev)
{
    {
	int i;

	for (i = 0; i < pdev->devn_params.separations.num_separations; ++i) {
	    RELOC_PTR(pdf14_device, devn_params.separations.names[i].data);
	}
    }
    RELOC_PTR(pdf14_device, devn_params.compressed_color_list);
    RELOC_VAR(pdev->ctx);
    pdev->target = gx_device_reloc_ptr(pdev->target, gcst);
}
RELOC_PTRS_END

/* ------ Private definitions ------ */

/**
 * pdf14_buf_new: Allocate a new PDF 1.4 buffer.
 * @n_chan: Number of pixel channels including alpha.
 *
 * Return value: Newly allocated buffer, or NULL on failure.
 **/
static	pdf14_buf *
pdf14_buf_new(gs_int_rect *rect, bool has_alpha_g, bool	has_shape,
	       int n_chan,
	       gs_memory_t *memory)
{
    pdf14_buf *result;
    int rowstride = (rect->q.x - rect->p.x + 3) & -4;
    int height = (rect->q.y - rect->p.y);
    int n_planes = n_chan + (has_shape ? 1 : 0) + (has_alpha_g ? 1 : 0);
    int planestride;
    double dsize = (((double) rowstride) * height) * n_planes;

    if (dsize > (double)max_uint)
      return NULL;

    result = gs_alloc_struct(memory, pdf14_buf, &st_pdf14_buf,
			     "pdf14_buf_new");
    if (result == NULL)
	return result;

    result->isolated = false;
    result->knockout = false;
    result->has_alpha_g = has_alpha_g;
    result->has_shape = has_shape;
    result->rect = *rect;
    result->n_chan = n_chan;
    result->n_planes = n_planes;
    result->rowstride = rowstride;
    result->transfer_fn = NULL;
    
    if (height <= 0) {
	/* Empty clipping - will skip all drawings. */
	result->planestride = 0;
	result->data = 0;
    } else {
	planestride = rowstride * height;
	result->planestride = planestride;
	result->data = gs_alloc_bytes(memory, planestride * n_planes,
					"pdf14_buf_new");
	if (result->data == NULL) {
	    gs_free_object(memory, result, "pdf_buf_new");
	    return NULL;
	}
	if (has_alpha_g) {
	    int alpha_g_plane = n_chan + (has_shape ? 1 : 0);
	    memset (result->data + alpha_g_plane * planestride, 0, planestride);
	}
    }
    /* Initialize bbox with the reversed rectangle for further accumulation : */
    result->bbox.p.x = rect->q.x;
    result->bbox.p.y = rect->q.y;
    result->bbox.q.x = rect->p.x;
    result->bbox.q.y = rect->p.y;
    return result;
}

static	void
pdf14_buf_free(pdf14_buf *buf, gs_memory_t *memory)
{
    gs_free_object(memory, buf->transfer_fn, "pdf14_buf_free");
    gs_free_object(memory, buf->data, "pdf14_buf_free");
    gs_free_object(memory, buf, "pdf14_buf_free");
}

static	pdf14_ctx *
pdf14_ctx_new(gs_int_rect *rect, int n_chan, bool additive, gs_memory_t	*memory)
{
    pdf14_ctx *result;
    pdf14_buf *buf;

    result = gs_alloc_struct(memory, pdf14_ctx, &st_pdf14_ctx,
			     "pdf14_ctx_new");
    if (result == NULL)
	return result;

    buf = pdf14_buf_new(rect, false, false, n_chan, memory);
    if (buf == NULL) {
	gs_free_object(memory, result, "pdf14_ctx_new");
	return NULL;
    }
    if_debug3('v', "[v]base buf: %d x %d, %d channels\n",
	      buf->rect.q.x, buf->rect.q.y, buf->n_chan);
    if (buf->data != NULL)
	memset(buf->data, 0, buf->planestride * buf->n_planes);
    buf->saved = NULL;
    result->stack = buf;
    result->maskbuf = NULL;
    result->n_chan = n_chan;
    result->memory = memory;
    result->rect = *rect;
    result->additive = additive;
    return result;
}

static	void
pdf14_ctx_free(pdf14_ctx *ctx)
{
    pdf14_buf *buf, *next;

    for (buf = ctx->stack; buf != NULL; buf = next) {
	next = buf->saved;
	pdf14_buf_free(buf, ctx->memory);
    }
    gs_free_object (ctx->memory, ctx, "pdf14_ctx_free");
}

/**
 * pdf14_find_backdrop_buf: Find backdrop buffer.
 *
 * Return value: Backdrop buffer for current group operation, or NULL
 * if backdrop is fully transparent.
 **/
static	pdf14_buf *
pdf14_find_backdrop_buf(pdf14_ctx *ctx)
{
    pdf14_buf *buf = ctx->stack;

    while (buf != NULL) {
	if (buf->isolated) return NULL;
	if (!buf->knockout) return buf->saved;
	buf = buf->saved;
    }
    /* this really shouldn't happen, as bottom-most buf should be
       non-knockout */
    return NULL;
}

static	int
pdf14_push_transparency_group(pdf14_ctx	*ctx, gs_int_rect *rect,
			      bool isolated, bool knockout,
			      byte alpha, byte shape,
			      gs_blend_mode_t blend_mode)
{
    pdf14_buf *tos = ctx->stack;
    pdf14_buf *buf, *backdrop;
    bool has_shape;

    /* todo: fix this hack, which makes all knockout groups isolated.
       For the vast majority of files, there won't be any visible
       effects, but it still isn't correct. The pixel compositing code
       for non-isolated knockout groups gets pretty hairy, which is
       why this is here. */
    if (knockout) 
	isolated = true;

    has_shape = tos->has_shape || tos->knockout;

    buf = pdf14_buf_new(rect, !isolated, has_shape, ctx->n_chan, ctx->memory);
    if_debug3('v', "[v]push buf: %d x %d, %d channels\n", buf->rect.p.x, buf->rect.p.y, buf->n_chan);
    if (buf == NULL)
	return_error(gs_error_VMerror);
    buf->isolated = isolated;
    buf->knockout = knockout;
    buf->alpha = alpha;
    buf->shape = shape;
    buf->blend_mode = blend_mode;

    buf->saved = tos;
    ctx->stack = buf;

    if (buf->data == NULL)
	return 0;

    backdrop = pdf14_find_backdrop_buf(ctx);
    if (backdrop == NULL) {
	memset(buf->data, 0, buf->planestride * (buf->n_chan +
						 (buf->has_shape ? 1 : 0)));
    } else {
	/* make copy of backdrop for compositing */
	int x0 = max(buf->rect.p.x, tos->rect.p.x);
	int x1 = min(buf->rect.q.x, tos->rect.q.x);
	int y0 = max(buf->rect.p.y, tos->rect.p.y);
	int y1 = min(buf->rect.q.y, tos->rect.q.y);

	if (x0 < x1 && y0 < y1) {
	    int width = x1 - x0;
	    byte *buf_plane = buf->data + x0 - buf->rect.p.x + (y0 - buf->rect.p.y) * buf->rowstride;
	    byte *tos_plane = tos->data + x0 - tos->rect.p.x + (y0 - tos->rect.p.y) * tos->rowstride;
	    int i;
	    int n_chan_copy = buf->n_chan + (tos->has_shape ? 1 : 0);

	    for (i = 0; i < n_chan_copy; i++) {
			byte *buf_ptr = buf_plane;
			byte *tos_ptr = tos_plane;
			int y;
	
			for (y = y0; y < y1; ++y) {
				memcpy (buf_ptr, tos_ptr, width); 
				buf_ptr += buf->rowstride;
				tos_ptr += tos->rowstride;
			}
			buf_plane += buf->planestride;
			tos_plane += tos->planestride;
		}
	    if (has_shape && !tos->has_shape)
			memset (buf_plane, 0, buf->planestride);
		}
	}

	return 0;
}

static	int
pdf14_pop_transparency_group(pdf14_ctx *ctx,
    const pdf14_nonseparable_blending_procs_t * pblend_procs)
{
    pdf14_buf *tos = ctx->stack;
    pdf14_buf *nos = tos->saved;
    pdf14_buf *maskbuf = ctx->maskbuf;
    int y0 = max(tos->rect.p.y, nos->rect.p.y);
    int y1 = min(tos->rect.q.y, nos->rect.q.y);
    int x0 = max(tos->rect.p.x, nos->rect.p.x);
    int x1 = min(tos->rect.q.x, nos->rect.q.x);
    if (x0 < x1 && y0 < y1) {
	int n_chan = ctx->n_chan;
	int num_comp = n_chan - 1;
	byte alpha = tos->alpha;
	byte shape = tos->shape;
	byte blend_mode = tos->blend_mode;
	byte *tos_ptr = tos->data + x0 - tos->rect.p.x +
	    (y0 - tos->rect.p.y) * tos->rowstride;
	byte *nos_ptr = nos->data + x0 - nos->rect.p.x +
	    (y0 - nos->rect.p.y) * nos->rowstride;
	byte *mask_ptr = NULL;
	int tos_planestride = tos->planestride;
	int nos_planestride = nos->planestride;
	int mask_planestride = 0x0badf00d; /* Quiet compiler. */
	byte mask_bg_alpha = 0; /* Quiet compiler. */
	int width = x1 - x0;
	int x, y;
	int i;
	byte tos_pixel[PDF14_MAX_PLANES];
	byte nos_pixel[PDF14_MAX_PLANES];
	bool tos_isolated = tos->isolated;
	bool nos_knockout = nos->knockout;
	byte *nos_alpha_g_ptr;
	int tos_shape_offset = n_chan * tos_planestride;
	int tos_alpha_g_offset = tos_shape_offset +
	(tos->has_shape ? tos_planestride : 0);
	int nos_shape_offset = n_chan * nos_planestride;
	bool nos_has_shape = nos->has_shape;
	byte *mask_tr_fn = NULL; /* Quiet compiler. */
	bool additive = ctx->additive;

	if (nos == NULL)
	    return_error(gs_error_rangecheck);

	rect_merge(nos->bbox, tos->bbox);

	if_debug6('v', "pdf14_pop_transparency_group y0 = %d, y1 = %d, w = %d, alpha = %d, shape = %d, bm = %d\n",
			    y0, y1, width, alpha, shape, blend_mode);
	if (nos->has_alpha_g)
	    nos_alpha_g_ptr = nos_ptr + n_chan * nos_planestride;
	else
	    nos_alpha_g_ptr = NULL;

	if (maskbuf != NULL) {
	    mask_ptr = maskbuf->data + x0 - maskbuf->rect.p.x +
		    (y0 - maskbuf->rect.p.y) * maskbuf->rowstride;
	    mask_planestride = maskbuf->planestride;
	    mask_bg_alpha = maskbuf->alpha;
	    mask_tr_fn = maskbuf->transfer_fn;
	}

	for (y = y0; y < y1; ++y) {
	    for (x = 0; x < width; ++x) {
		byte pix_alpha = alpha;

		/* Complement the components for subtractive color spaces */
		if (additive) {
		    for (i = 0; i < n_chan; ++i) {
			tos_pixel[i] = tos_ptr[x + i * tos_planestride];
			nos_pixel[i] = nos_ptr[x + i * nos_planestride];
		    }
		} else {
		    for (i = 0; i < num_comp; ++i) {
			tos_pixel[i] = 255 - tos_ptr[x + i * tos_planestride];
			nos_pixel[i] = 255 - nos_ptr[x + i * nos_planestride];
		    }
		    tos_pixel[num_comp] = tos_ptr[x + num_comp * tos_planestride];
		    nos_pixel[num_comp] = nos_ptr[x + num_comp * nos_planestride];
		}

		if (mask_ptr != NULL) {
		    int mask_alpha = mask_ptr[x + num_comp * mask_planestride];
		    int tmp;
		    byte mask;

			/*
			* The mask data is really monochrome.  Thus for additive (RGB)
			* we use the R channel for alpha since R = G = B.  For
			* subtractive (CMYK) we use the K channel.
			*/
		    if (mask_alpha == 255) {
			/* todo: rgba->mask */
			mask = additive ? mask_ptr[x]
					: 255 - mask_ptr[x + 3 * mask_planestride];
		    } else if (mask_alpha == 0)
			mask = mask_bg_alpha;
		    else {
			int t2 = additive ? mask_ptr[x]
					: 255 - mask_ptr[x + 3 * mask_planestride];

			t2 = (t2 - mask_bg_alpha) * mask_alpha + 0x80;
			mask = mask_bg_alpha + ((t2 + (t2 >> 8)) >> 8);
		    }
		    mask = mask_tr_fn[mask];
		    tmp = pix_alpha * mask + 0x80;
		    pix_alpha = (tmp + (tmp >> 8)) >> 8;
		}

		if (nos_knockout) {
		    byte *nos_shape_ptr = nos_has_shape ?
			&nos_ptr[x + nos_shape_offset] : NULL;
		    byte tos_shape = tos_ptr[x + tos_shape_offset];

		    art_pdf_composite_knockout_isolated_8(nos_pixel,
							nos_shape_ptr,
							tos_pixel,
							n_chan - 1,
							tos_shape,
							pix_alpha, shape);
		} else if (tos_isolated) {
		    art_pdf_composite_group_8(nos_pixel, nos_alpha_g_ptr,
					tos_pixel, n_chan - 1,
					pix_alpha, blend_mode, pblend_procs);
		} else {
		    byte tos_alpha_g = tos_ptr[x + tos_alpha_g_offset];
		    art_pdf_recomposite_group_8(nos_pixel, nos_alpha_g_ptr,
					tos_pixel, tos_alpha_g, n_chan - 1,
					pix_alpha, blend_mode, pblend_procs);
		}
		if (nos_has_shape) {
		    nos_ptr[x + nos_shape_offset] =
			art_pdf_union_mul_8 (nos_ptr[x + nos_shape_offset],
						tos_ptr[x + tos_shape_offset],
						shape);
		}
	    
		/* Complement the results for subtractive color spaces */
		if (additive) {
		    for (i = 0; i < n_chan; ++i) {
			nos_ptr[x + i * nos_planestride] = nos_pixel[i];
		    }
		} else {
		    for (i = 0; i < num_comp; ++i)
			nos_ptr[x + i * nos_planestride] = 255 - nos_pixel[i];
		    nos_ptr[x + num_comp * nos_planestride] = nos_pixel[num_comp];
		}
		if (nos_alpha_g_ptr != NULL)
		    ++nos_alpha_g_ptr;
	    }
	    tos_ptr += tos->rowstride;
	    nos_ptr += nos->rowstride;
	    if (nos_alpha_g_ptr != NULL)
		nos_alpha_g_ptr += nos->rowstride - width;
	    if (mask_ptr != NULL)
		mask_ptr += maskbuf->rowstride;
	}
    }

    ctx->stack = nos;
    if_debug0('v', "[v]pop buf\n");
    pdf14_buf_free(tos, ctx->memory);
    if (maskbuf != NULL) {
	pdf14_buf_free(maskbuf, ctx->memory);
	ctx->maskbuf = NULL;
    }
    return 0;
}

static	int
pdf14_push_transparency_mask(pdf14_ctx *ctx, gs_int_rect *rect,	byte bg_alpha,
			     byte *transfer_fn)
{
    pdf14_buf *buf;

    if_debug0('v', "[v]pdf_push_transparency_mask\n");
    buf = pdf14_buf_new(rect, false, false, ctx->n_chan, ctx->memory);
    if (buf == NULL)
	return_error(gs_error_VMerror);

    buf->alpha = bg_alpha;

    /* fill in, but these values aren't really used */
    buf->isolated = true;
    buf->knockout = false;
    buf->shape = 0xff;
    buf->blend_mode = BLEND_MODE_Normal;
    buf->transfer_fn = transfer_fn;

    buf->saved = ctx->stack;
    ctx->stack = buf;
    if (buf->data != NULL)
	memset(buf->data, 0, buf->planestride * buf->n_chan);
    return 0;
}

static	int
pdf14_pop_transparency_mask(pdf14_ctx *ctx)
{
    pdf14_buf *tos = ctx->stack;

    ctx->stack = tos->saved;
    ctx->maskbuf = tos;
    return 0;
}

static	int
pdf14_open(gx_device *dev)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    gs_int_rect rect;

    if_debug2('v', "[v]pdf14_open: width = %d, height = %d\n",
	     dev->width, dev->height);

    rect.p.x = 0;
    rect.p.y = 0;
    rect.q.x = dev->width;
    rect.q.y = dev->height;
    pdev->ctx = pdf14_ctx_new(&rect, dev->color_info.num_components + 1,
	pdev->color_info.polarity != GX_CINFO_POLARITY_SUBTRACTIVE, dev->memory);
    if (pdev->ctx == NULL)
	return_error(gs_error_VMerror);
    return 0;
}

/*
 * Encode a list of colorant values into a gx_color_index_value.
 */
static	gx_color_index
pdf14_encode_color(gx_device *dev, const gx_color_value	colors[])
{
    int drop = sizeof(gx_color_value) * 8 - 8;
    gx_color_index color = 0;
    int i;
    int ncomp = dev->color_info.num_components;

    for (i = 0; i < ncomp; i++) {
	color <<= 8;
	color |= (colors[i] >> drop);
    }
    return (color == gx_no_color_index ? color ^ 1 : color);
}

/*
 * Decode a gx_color_index value back to a list of colorant values.
 */
static	int
pdf14_decode_color(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    int i;
    int ncomp = dev->color_info.num_components;

    for (i = 0; i < ncomp; i++) {
	out[ncomp - i - 1] = (gx_color_value) ((color & 0xff) * 0x101);
	color >>= 8;
    }
    return 0;
}

/*
 * Encode a list of colorant values into a gx_color_index_value.  For more
 * information about 'compressed' color index values see the comments before
 * the devn_encode_compressed_color routine.
 */
static gx_color_index
pdf14_compressed_encode_color(gx_device *dev, const gx_color_value colors[])
{
    return devn_encode_compressed_color(dev, colors,
		    &(((pdf14_device *)dev)->devn_params));
}

/*
 * Decode a gx_color_index value back to a list of colorant values.  For more
 * information about 'compressed' color index values see the comments before
 * the devn_encode_compressed_color routine.
 */
static int
pdf14_compressed_decode_color(gx_device * dev, gx_color_index color,
	       						gx_color_value * out)
{
    return devn_decode_compressed_color(dev, color, out,
		    &(((pdf14_device *)dev)->devn_params));
}

static void
pdf14_gray_cs_to_cmyk_cm(gx_device * dev, frac gray, frac out[])
{
    int num_comp = dev->color_info.num_components;

    out[0] = out[1] = out[2] = frac_0;
    out[3] = frac_1 - gray;
    for (--num_comp; num_comp > 3; num_comp--)
	out[num_comp] = 0;
}

/*
 * Default map from DeviceRGB color space to DeviceCMYK color
 * model. Since this mapping is defined by the PostScript language
 * it is unlikely that any device with a DeviceCMYK color model
 * would define this mapping on its own.
 *
 * If the imager state is not available, map as though the black
 * generation and undercolor removal functions are identity
 * transformations. This mode is used primarily to support the
 * raster operation (rop) feature of PCL, which requires that
 * the raster operation be performed in an RGB color space.
 * Note that default black generation and undercolor removal
 * functions in PostScript need NOT be identity transformations:
 * often they are { pop 0 }.
 */
static void
pdf14_rgb_cs_to_cmyk_cm(gx_device * dev, const gs_imager_state *pis,
  			   frac r, frac g, frac b, frac out[])
{
    int num_comp = dev->color_info.num_components;

    if (pis != 0)
        color_rgb_to_cmyk(r, g, b, pis, out);
    else {
        frac    c = frac_1 - r, m = frac_1 - g, y = frac_1 - b;
        frac    k = min(c, min(m, g));

        out[0] = c - k;
        out[1] = m - k;
        out[2] = y - k;
        out[3] = k;
    }
    for (--num_comp; num_comp > 3; num_comp--)
	out[num_comp] = 0;
}

void
pdf14_cmyk_cs_to_cmyk_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    int num_comp = dev->color_info.num_components;

    out[0] = c;
    out[1] = m;
    out[2] = y;
    out[3] = k;
    for (--num_comp; num_comp > 3; num_comp--)
	out[num_comp] = 0;
}
static const gx_cm_color_map_procs pdf14_DeviceCMYKspot_procs = {
    pdf14_gray_cs_to_cmyk_cm, pdf14_rgb_cs_to_cmyk_cm, pdf14_cmyk_cs_to_cmyk_cm
};

const gx_cm_color_map_procs *
pdf14_cmykspot_get_color_mapping_procs(const gx_device * dev)
{
    return &pdf14_DeviceCMYKspot_procs;
}

#ifdef DUMP_TO_PNG
/* Dumps a planar RGBA image to	a PNG file. */
static	int
dump_planar_rgba(gs_memory_t *mem, const pdf14_buf *pbuf)
		 const byte *buf, int width, int height, int rowstride, int planestride)
{
    int rowstride = pbuf->rowstride, planestride = pbuf->planestride;
    int rowbytes = width << 2;
    gs_int_rect rect = buf->rect;
    int x1 = min(pdev->width, rect.q.x);
    int y1 = min(pdev->height, rect.q.y);
    int width = x1 - rect.p.x;
    int height = y1 - rect.p.y;
    byte *buf_ptr = buf->data + rect.p.y * buf->rowstride + rect.p.x;
    byte *row = gs_malloc(mem, rowbytes, 1, "png raster buffer");
    png_struct *png_ptr =
    png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_info *info_ptr =
    png_create_info_struct(png_ptr);
    const char *software_key = "Software";
    char software_text[256];
    png_text text_png;
    FILE *file;
    int code;
    int y;

    if (buf->data == NULL)
	return 0;

    file = fopen ("c:\\temp\\tmp.png", "wb");

    if_debug0('v', "[v]pnga_output_page\n");

    if (row == 0 || png_ptr == 0 || info_ptr == 0) {
	code = gs_note_error(gs_error_VMerror);
	goto done;
    }
    /* set error handling */
    if (setjmp(png_ptr->jmpbuf)) {
	/* If we get here, we had a problem reading the file */
	code = gs_note_error(gs_error_VMerror);
	goto done;
    }

    code = 0;			/* for normal path */
    /* set up the output control */
    png_init_io(png_ptr, file);

    /* set the file information here */
    info_ptr->width = width;
    info_ptr->height = height;
    /* resolution is in pixels per meter vs. dpi */
    info_ptr->x_pixels_per_unit =
	(png_uint_32) (96.0 * (100.0 / 2.54));
    info_ptr->y_pixels_per_unit =
	(png_uint_32) (96.0 * (100.0 / 2.54));
    info_ptr->phys_unit_type = PNG_RESOLUTION_METER;
    info_ptr->valid |= PNG_INFO_pHYs;

    /* At present, only supporting 32-bit rgba */
    info_ptr->bit_depth = 8;
    info_ptr->color_type = PNG_COLOR_TYPE_RGB_ALPHA;

    /* add comment */
    sprintf(software_text, "%s %d.%02d", gs_product,
	    (int)(gs_revision / 100), (int)(gs_revision % 100));
    text_png.compression = -1;	/* uncompressed */
    text_png.key = (char *)software_key;	/* not const, unfortunately */
    text_png.text = software_text;
    text_png.text_length = strlen(software_text);
    info_ptr->text = &text_png;
    info_ptr->num_text = 1;

    /* write the file information */
    png_write_info(png_ptr, info_ptr);

    /* don't write the comments twice */
    info_ptr->num_text = 0;
    info_ptr->text = NULL;

    /* Write the contents of the image. */
    for (y = 0; y < height; ++y) {
	int x;

	for (x = 0; x < width; ++x) {
	    row[(x << 2)] = buf_ptr[x];
	    row[(x << 2) + 1] = buf_ptr[x + planestride];
	    row[(x << 2) + 2] = buf_ptr[x + planestride * 2];
	    row[(x << 2) + 3] = buf_ptr[x + planestride * 3];
	}
	png_write_row(png_ptr, row);
	buf_ptr += rowstride;
    }

    /* write the rest of the file */
    png_write_end(png_ptr, info_ptr);

  done:
    /* free the structures */
    png_destroy_write_struct(&png_ptr, &info_ptr);
    gs_free(mem, row, rowbytes, 1, "png raster buffer");

    fclose (file);
    return code;
}
#endif


/**
 * pdf14_put_image: Put rendered image to target device.
 * @pdev: The PDF 1.4 rendering device.
 * @pgs: State for image draw operation.
 * @target: The target device.
 *
 * Puts the rendered image in @pdev's buffer to @target. This is called
 * as part of the sequence of popping the PDF 1.4 device filter.
 *
 * Return code: negative on error.
 **/
static	int
pdf14_put_image(gx_device * dev, gs_imager_state * pis, gx_device * target)
{
    const pdf14_device * pdev = (pdf14_device *)dev;
    int code;
    gs_image1_t image;
    gs_matrix pmat;
    gx_image_enum_common_t *info;
    pdf14_buf *buf = pdev->ctx->stack;
    gs_int_rect rect = buf->rect;
    int y;
    int planestride = buf->planestride;
    int num_comp = buf->n_chan - 1;
    byte *linebuf;
    gs_color_space *pcs;
    const byte bg = pdev->ctx->additive ? 255 : 0;
    int x1, y1, width, height;
    byte *buf_ptr;

    if_debug0('v', "[v]pdf14_put_image\n");
    rect_intersect(rect, buf->bbox);
    x1 = min(pdev->width, rect.q.x);
    y1 = min(pdev->height, rect.q.y);
    width = x1 - rect.p.x;
    height = y1 - rect.p.y;
#ifdef DUMP_TO_PNG
    dump_planar_rgba(pdev->memory, buf);
#endif
    if (width <= 0 || height <= 0 || buf->data == NULL)
	return 0;
    buf_ptr = buf->data + rect.p.y * buf->rowstride + rect.p.x;

#if 0
    /* Set graphics state device to target, so that image can set up
       the color mapping properly. */
    rc_increment(pdev);
    gs_setdevice_no_init(pgs, target);
#endif

    /*
     * Set color space to either Gray, RGB, or CMYK in preparation for sending
     * an image.
     */
    switch (num_comp) {
	case 1:				/* DeviceGray */
	    pcs = gs_cspace_new_DeviceGray(pis->memory);
	    break;
	case 3:				/* DeviceRGB */
	    pcs = gs_cspace_new_DeviceRGB(pis->memory);
	    break;
	case 4:				/* DeviceCMYK */
	    pcs = gs_cspace_new_DeviceCMYK(pis->memory);
	    break;
	default:			/* Should never occur */
	    return_error(gs_error_rangecheck);
	    break;
    }
    if (pcs == NULL)
	return_error(gs_error_VMerror);
    gs_image_t_init_adjust(&image, pcs, false);
    image.ImageMatrix.xx = (float)width;
    image.ImageMatrix.yy = (float)height;
    image.Width = width;
    image.Height = height;
    image.BitsPerComponent = 8;
    pmat.xx = (float)width;
    pmat.xy = 0;
    pmat.yx = 0;
    pmat.yy = (float)height;
    pmat.tx = (float)rect.p.x;
    pmat.ty = (float)rect.p.y;
    code = dev_proc(target, begin_typed_image) (target,
						pis, &pmat,
						(gs_image_common_t *)&image,
						NULL, NULL, NULL,
						pis->memory, &info);
    if (code < 0) {
	rc_decrement_only(pcs, "pdf14_put_image");
	return code;
    }

    linebuf = gs_alloc_bytes(pdev->memory, width * num_comp, "pdf14_put_image");
    for (y = 0; y < height; y++) {
	gx_image_plane_t planes;
	int x;
	int rows_used;

	for (x = 0; x < width; x++) {
	    byte comp, a;
	    int tmp, comp_num;

	    /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
	    a = buf_ptr[x + planestride * num_comp];

	    if ((a + 1) & 0xfe) {
		a ^= 0xff;
		for (comp_num = 0; comp_num < num_comp; comp_num++) {
		    comp  = buf_ptr[x + planestride * comp_num];
		    tmp = ((bg - comp) * a) + 0x80;
		    comp += (tmp + (tmp >> 8)) >> 8;
		    linebuf[x * num_comp + comp_num] = comp;
		}
	    } else if (a == 0) {
		for (comp_num = 0; comp_num < num_comp; comp_num++) {
		    linebuf[x * num_comp + comp_num] = bg;
		}
	    } else {
		for (comp_num = 0; comp_num < num_comp; comp_num++) {
		    comp = buf_ptr[x + planestride * comp_num];
		    linebuf[x * num_comp + comp_num] = comp;
		}
	    }
	}

	planes.data = linebuf;
	planes.data_x = 0;
	planes.raster = width * num_comp;
	info->procs->plane_data(info, &planes, 1, &rows_used);
	/* todo: check return value */

	buf_ptr += buf->rowstride;
    }
    gs_free_object(pdev->memory, linebuf, "pdf14_put_image");

    info->procs->end_image(info, true);

#if 0
    /* Restore device in graphics state.*/
    gs_setdevice_no_init(pgs, (gx_device*) pdev);
    rc_decrement_only(pdev, "pdf_14_put_image");
#endif

    rc_decrement_only(pcs, "pdf14_put_image");

    return code;
}

/**
 * pdf14_cmykspot_put_image: Put rendered image to target device.
 * @pdev: The PDF 1.4 rendering device.
 * @pis: State for image draw operation.
 * @target: The target device.
 *
 * Puts the rendered image in @pdev's buffer to @target. This is called
 * as part of the sequence of popping the PDF 1.4 device filter.
 *
 * Return code: negative on error.
 **/
static	int
pdf14_cmykspot_put_image(gx_device * dev, gs_imager_state * pis, gx_device * target)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    int code = 0;
    int x, y, tmp, comp_num, output_comp_num;
    pdf14_buf *buf = pdev->ctx->stack;
    gs_int_rect rect = buf->rect;
    int planestride = buf->planestride;
    int num_comp = buf->n_chan - 1;
    const byte bg = pdev->ctx->additive ? gx_max_color_value : 0;
    gx_color_index color;
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value comp;
    byte a;
    int input_map[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int output_map[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int num_known_comp = 0;
    int output_num_comp = target->color_info.num_components;
    gs_devn_params * pdevn_params = &pdev->devn_params;
    gs_separations * pseparations = &pdevn_params->separations;
    int num_sep = pseparations->num_separations++;
    int x1, y1, width, height;
    byte *buf_ptr;

    if_debug0('v', "[v]pdf14_cmykspot_put_image\n");
    rect_intersect(rect, buf->bbox);
    x1 = min(pdev->width, rect.q.x);
    y1 = min(pdev->height, rect.q.y);
    width = x1 - rect.p.x;
    height = y1 - rect.p.y;
    if (width <= 0 || height <= 0 || buf->data == NULL)
	return 0;
    buf_ptr = buf->data + rect.p.y * buf->rowstride + rect.p.x;

    /*
     * The process color model for the PDF 1.4 compositor device is CMYK plus
     * spot colors.  The target device may have only some of these colorants due
     * to the SeparationOrder device parameter.  Thus we need to determine the
     * mapping between the PDF 1.4 compositor and the target device.  Note:
     * There should not be a spot colorant in the PDF 1.4 device that is not
     * present in the target device.
     */
    /* Check if target processes CMYK colorants. */
    for (comp_num = 0; comp_num < 4; comp_num++) {
	const char * pcomp_name = (const char *)DeviceCMYKComponents[comp_num]; 

	output_comp_num = dev_proc(target, get_color_comp_index)
		(target, pcomp_name, strlen(pcomp_name), NO_COMP_NAME_TYPE);
	if (output_comp_num >=0 &&
	     	output_comp_num < GX_DEVICE_COLOR_MAX_COMPONENTS) {
	    output_map[num_known_comp] = output_comp_num;
	    input_map[num_known_comp++] = comp_num;
	}
    }
    /* Check if target processes our spot colorants. */
    for (comp_num = 0; comp_num < num_sep; comp_num++) {
	output_comp_num = dev_proc(target, get_color_comp_index)
	       (target, (const char *)(pseparations->names[comp_num].data),
	       	pseparations->names[comp_num].size,  NO_COMP_NAME_TYPE);
	if (output_comp_num >= 0 &&
	     	output_comp_num < GX_DEVICE_COLOR_MAX_COMPONENTS) {
	    output_map[num_known_comp] = output_comp_num;
	    input_map[num_known_comp++] = comp_num + 4;
	}
    }

    /* Clear all output colorants first */
    for (comp_num = 0; comp_num < output_num_comp; comp_num++)
	cv[comp_num] = 0;

    /* Send pixel data to the target device. */
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {

	    /* composite CMYKA, etc. pixel with over solid background */
	    a = buf_ptr[x + planestride * num_comp];

	    if ((a + 1) & 0xfe) {
		a ^= 0xff;
		for (comp_num = 0; comp_num < num_known_comp; comp_num++) {
		    comp  = buf_ptr[x + planestride * input_map[comp_num]];
		    tmp = ((comp - bg) * a) + 0x80;
		    comp += tmp + (tmp >> 8);
		    cv[output_map[comp_num]] = comp;
		}
	    } else if (a == 0) {
		for (comp_num = 0; comp_num < num_known_comp; comp_num++) {
		    cv[output_map[comp_num]] = bg;
		}
	    } else {
		for (comp_num = 0; comp_num < num_known_comp; comp_num++) {
		    comp = buf_ptr[x + planestride * input_map[comp_num]];
		    cv[output_map[comp_num]] = (comp << 8) + comp;
		}
	    }
	    color = dev_proc(target, encode_color)(target, cv);
	    code = dev_proc(target, fill_rectangle)(target, x + rect.p.x, 
							    y + rect.p.y, 1, 1, color);
	}

	buf_ptr += buf->rowstride;
    }

    return code;
}

/**
 * pdf14_custom_put_image: Put rendered image to target device.
 * @pdev: The PDF 1.4 rendering device.
 * @pis: State for image draw operation.
 * @target: The target device.
 *
 * Puts the rendered image in @pdev's buffer to @target. This is called
 * as part of the sequence of popping the PDF 1.4 device filter.
 *
 * Return code: negative on error.
 **/
static	int
pdf14_custom_put_image(gx_device * dev, gs_imager_state * pis, gx_device * target)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    int code = 0;
    int x, y, tmp, comp_num;
    pdf14_buf *buf = pdev->ctx->stack;
    gs_int_rect rect = buf->rect;
    int planestride = buf->planestride;
    int num_comp = buf->n_chan - 1;
    const byte bg = pdev->ctx->additive ? gx_max_color_value : 0;
    gx_color_index color;
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value comp;
    byte a;
    int x1, y1, width, height;
    byte *buf_ptr;

    if_debug0('v', "[v]pdf14_custom_put_image\n");
    rect_intersect(rect, buf->bbox);
    x1 = min(pdev->width, rect.q.x);
    y1 = min(pdev->height, rect.q.y);
    width = x1 - rect.p.x;
    height = y1 - rect.p.y;
    if (width <= 0 || height <= 0 || buf->data == NULL)
	return 0;
    buf_ptr = buf->data + rect.p.y * buf->rowstride + rect.p.x;

    /* Send pixel data to the target device. */
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {

	    /* composite CMYKA, etc. pixel with over solid background */
	    a = buf_ptr[x + planestride * num_comp];

	    if ((a + 1) & 0xfe) {
		a ^= 0xff;
		for (comp_num = 0; comp_num < num_comp; comp_num++) {
		    comp  = buf_ptr[x + planestride * comp_num];
		    tmp = ((bg - comp) * a) + 0x80;
		    comp += tmp + (tmp >> 8);
		    cv[comp_num] = comp;
		}
	    } else if (a == 0) {
		for (comp_num = 0; comp_num < num_comp; comp_num++) {
		    cv[comp_num] = bg;
		}
	    } else {
		for (comp_num = 0; comp_num < num_comp; comp_num++) {
		    comp = buf_ptr[x + planestride * comp_num];
		    cv[comp_num] = (comp << 8) + comp;
		}
	    }
	    color = dev_proc(target, encode_color)(target, cv);
	    code = dev_proc(target, fill_rectangle)(target, x + rect.p.x, 
							    y + rect.p.y, 1, 1, color);
	}

	buf_ptr += buf->rowstride;
    }

    return code;
}

static	int
pdf14_close(gx_device *dev)
{
    pdf14_device *pdev = (pdf14_device *)dev;

    if (pdev->ctx) {
	pdf14_ctx_free(pdev->ctx);
	pdev->ctx = NULL;
    }
    return 0;
}

static	int
pdf14_output_page(gx_device * dev, int num_copies, int flush)
{
    pdf14_device * pdev = (pdf14_device *)dev;

    if (pdev->target != NULL)
	return (*dev_proc(pdev->target, output_page)) (pdev->target, num_copies, flush);
    return 0;
}

#define	COPY_PARAM(p) dev->p = target->p
#define	COPY_ARRAY_PARAM(p) memcpy(dev->p, target->p, sizeof(dev->p))

/*
 * Copy device parameters back from a target.  This copies all standard
 * parameters related to page size and resolution, but not any of the
 * color-related parameters, as the pdf14 device retains its own color
 * handling. This routine is parallel to gx_device_copy_params().
 */
static	void
gs_pdf14_device_copy_params(gx_device *dev, const gx_device *target)
{
	COPY_PARAM(width);
	COPY_PARAM(height);
	COPY_ARRAY_PARAM(MediaSize);
	COPY_ARRAY_PARAM(ImagingBBox);
	COPY_PARAM(ImagingBBox_set);
	COPY_ARRAY_PARAM(HWResolution);
	COPY_ARRAY_PARAM(MarginsHWResolution);
	COPY_ARRAY_PARAM(Margins);
	COPY_ARRAY_PARAM(HWMargins);
	COPY_PARAM(PageCount);
#undef COPY_ARRAY_PARAM
#undef COPY_PARAM
}

/*
 * This is a forwarding version of the put_params device proc.  It is only
 * used when the PDF 1.4 compositor devices are closed.  The routine will
 * check if the target device has closed and, if so, close itself.  The routine
 * also sync the device parameters.
 */
static	int
pdf14_forward_put_params(gx_device * dev, gs_param_list	* plist)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    gx_device * tdev = pdev->target;
    int code = 0;

    if (tdev != 0 && (code = dev_proc(tdev, put_params)(tdev, plist)) >= 0) {
	gx_device_decache_colors(dev);
	if (!tdev->is_open)
	    code = gs_closedevice(dev);
	gx_device_copy_params(dev, tdev);
    }
    return code;
}

/* Function prototypes */
int put_param_compressed_color_list_elem(gx_device * pdev,
    gs_param_list * plist, compressed_color_list_t ** pret_comp_list,
    char * keyname, int num_comps);
int put_param_pdf14_spot_names(gx_device * pdev,
		gs_separations * pseparations, gs_param_list * plist);
#define PDF14CompressedColorListParamName "PDF14CompressedColorList"
#define PDF14NumSpotColorsParamName "PDF14NumSpotColors"

/*
 * The put_params method for the PDF 1.4 device will check if the
 * target device has closed and, if so, close itself.  Note:  This routine is
 * currently being used by both the pdf14_clist_device and the pdf_device.
 * Please make sure that any changes are either applicable to both devices
 * or clone the routine for each device.
 */
static	int
pdf14_put_params(gx_device * dev, gs_param_list	* plist)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    gx_device * tdev = pdev->target;
    int code = 0;

    if (tdev != 0 && (code = dev_proc(tdev, put_params)(tdev, plist)) >= 0) {
	gx_device_decache_colors(dev);
	if (!tdev->is_open)
	    code = gs_closedevice(dev);
	gs_pdf14_device_copy_params(dev, tdev);
    }
#if 0
    put_param_compressed_color_list_elem(pdev, plist, &pret_comp_list,
		  PDF14CompressedColorListParamName, TOP_ENCODED_LEVEL);
    put_param_pdf14_spot_names(dev, &pdev->devn_params, plist);
#endif
    return code;
}

/*
 * Copy marking related parameters into the PDF 1.4 device structure for use
 * by pdf14_fill_rrectangle.
 */
static	void
pdf14_set_marking_params(gx_device *dev, const gs_imager_state *pis)
{
    pdf14_device * pdev = (pdf14_device *)dev;

    pdev->opacity = pis->opacity.alpha;
    pdev->shape = pis->shape.alpha;
    pdev->alpha = pis->opacity.alpha * pis->shape.alpha;
    pdev->blend_mode = pis->blend_mode;
    if_debug3('v', "[v]set_marking_params, opacity = %g, shape = %g, bm = %d\n",
	      pdev->opacity, pdev->shape, pis->blend_mode);
}

static	int
pdf14_fill_path(gx_device *dev,	const gs_imager_state *pis,
			   gx_path *ppath, const gx_fill_params *params,
			   const gx_drawing_color *pdcolor,
			   const gx_clip_path *pcpath)
{
    gs_imager_state new_is = *pis;

    /*
     * The blend operations are not idempotent.  Force non-idempotent
     * filling and stroking operations.
     */
    new_is.log_op |= lop_pdf14;
    pdf14_set_marking_params(dev, pis);
    return gx_default_fill_path(dev, &new_is, ppath, params, pdcolor, pcpath);
}

static	int
pdf14_stroke_path(gx_device *dev, const	gs_imager_state	*pis,
			     gx_path *ppath, const gx_stroke_params *params,
			     const gx_drawing_color *pdcolor,
			     const gx_clip_path *pcpath)
{
    gs_imager_state new_is = *pis;

    /*
     * The blend operations are not idempotent.  Force non-idempotent
     * filling and stroking operations.
     */
    new_is.log_op |= lop_pdf14;
    pdf14_set_marking_params(dev, pis);
    return gx_default_stroke_path(dev, &new_is, ppath, params, pdcolor,
				  pcpath);
}

static	int
pdf14_begin_typed_image(gx_device * dev, const gs_imager_state * pis,
			   const gs_matrix *pmat, const gs_image_common_t *pic,
			   const gs_int_rect * prect,
			   const gx_drawing_color * pdcolor,
			   const gx_clip_path * pcpath, gs_memory_t * mem,
			   gx_image_enum_common_t ** pinfo)
{
    pdf14_set_marking_params(dev, pis);
    return gx_default_begin_typed_image(dev, pis, pmat, pic, prect, pdcolor,
					pcpath, mem, pinfo);
}

static	void
pdf14_set_params(gs_imager_state * pis,	gx_device * dev,
				const gs_pdf14trans_params_t * pparams)
{
    if_debug0('v', "[v]pdf14_set_params\n");
    if (pparams->changed & PDF14_SET_BLEND_MODE)
	pis->blend_mode = pparams->blend_mode;
    if (pparams->changed & PDF14_SET_TEXT_KNOCKOUT)
	pis->text_knockout = pparams->text_knockout;
    if (pparams->changed & PDF14_SET_SHAPE_ALPHA)
	pis->shape.alpha = pparams->shape.alpha;
    if (pparams->changed & PDF14_SET_OPACITY_ALPHA)
	pis->opacity.alpha = pparams->opacity.alpha;
    pdf14_set_marking_params(dev, pis);
}

/*
 * This open_device method for the PDF 1.4 compositor devices is only used
 * when these devices are disabled.  This routine is about as close to
 * a pure "forwarding" open_device operation as is possible. Its only
 * significant function is to ensure that the is_open field of the
 * PDF 1.4 compositor devices matches that of the target device.
 *
 * We assume this procedure is called only if the device is not already
 * open, and that gs_opendevice will take care of the is_open flag.
 */
static	int
pdf14_forward_open_device(gx_device * dev)
{
    gx_device_forward * pdev = (gx_device_forward *)dev;
    gx_device * tdev = pdev->target;
    int code = 0;

    /* The PDF 1.4 compositing devices must have a target */
    if (tdev == 0)
	return_error(gs_error_unknownerror);
    if ((code = gs_opendevice(tdev)) >= 0)
	gx_device_copy_params(dev, tdev);
    return code;
}

/*
 * Convert all device procs to be 'forwarding'.  The caller is responsible
 * for setting any device procs that should not be forwarded.
 */
static	void
pdf14_forward_device_procs(gx_device * dev)
{
    gx_device_forward * pdev = (gx_device_forward *)dev;

    /*
     * We are using gx_device_forward_fill_in_procs to set the various procs.
     * This will ensure that any new device procs are also set.  However that
     * routine only changes procs which are NULL.  Thus we start by setting all
     * procs to NULL.
     */
    memset(&(pdev->procs), 0, size_of(pdev->procs));
    gx_device_forward_fill_in_procs(pdev);
    /*
     * gx_device_forward_fill_in_procs does not forward all procs.
     * Set the remainding procs to also forward.
     */
    set_dev_proc(dev, close_device, gx_forward_close_device);
    set_dev_proc(dev, fill_rectangle, gx_forward_fill_rectangle);
    set_dev_proc(dev, tile_rectangle, gx_forward_tile_rectangle);
    set_dev_proc(dev, copy_mono, gx_forward_copy_mono);
    set_dev_proc(dev, copy_color, gx_forward_copy_color);
    set_dev_proc(dev, get_page_device, gx_forward_get_page_device);
    set_dev_proc(dev, strip_tile_rectangle, gx_forward_strip_tile_rectangle);
    set_dev_proc(dev, copy_alpha, gx_forward_copy_alpha);
    /* These are forwarding devices with minor tweaks. */
    set_dev_proc(dev, open_device, pdf14_forward_open_device);
    set_dev_proc(dev, put_params, pdf14_forward_put_params);
}

/*
 * Disable the PDF 1.4 compositor device.  Once created, the PDF 1.4
 * compositor device is never removed.  (We do not have a remove compositor
 * method.)  However it is no-op'ed when the PDF 1.4 device is popped.  This
 * routine implements that action.
 */
static	int
pdf14_disable_device(gx_device * dev)
{
    gx_device_forward * pdev = (gx_device_forward *)dev;

    if_debug0('v', "[v]pdf14_disable_device\n");
    dev->color_info = pdev->target->color_info;
    pdf14_forward_device_procs(dev);
    set_dev_proc(dev, create_compositor, pdf14_forward_create_compositor);
    return 0;
}

/*
 * The default color space for PDF 1.4 blend modes is based upon the process
 * color model of the output device.
 */
static	pdf14_default_colorspace_t
pdf14_determine_default_blend_cs(gx_device * pdev)
{
    if (pdev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
	/*
	 * Note:  We do not allow the SeparationOrder device parameter for
	 * additive devices.  Thus we always have 1 colorant for DeviceGray
	 * and 3 colorants for DeviceRGB.  We do not currently support
	 * blending in a DeviceGray color space.  Thus we oniy use DeviceRGB.
	 */
	return PDF14_DeviceRGB;
    else {
	/*
	 * Check if the device is CMYK only or CMYK plus spot colors.
	 */
	int i, output_comp_num, num_cmyk_used = 0, num_cmyk = 0;

#if CUSTOM_BLENDING_MODE == ALWAYS_USE_CUSTOM_BLENDING
	return PDF14_DeviceCustom;
#endif
	/*
	 * Count the number of CMYK process components supported by the output
	 * device.
	 */
	for (i = 0; i < 4; i++) {
	    const char * pcomp_name = (const char *)DeviceCMYKComponents[i]; 

	    output_comp_num = dev_proc(pdev, get_color_comp_index)
		(pdev, pcomp_name, strlen(pcomp_name), NO_COMP_NAME_TYPE);
	    
	    if (output_comp_num >= 0) {
		num_cmyk++;
		if (output_comp_num != GX_DEVICE_COLOR_MAX_COMPONENTS)
		    num_cmyk_used++;
	    }
	}
	/*
	 * Check if the device supports only CMYK.  Otherewise we assume that
	 * the output device supports spot colors.  Note:  This algorithm can
	 * be fooled if the SeparationOrder device parameter is being used by
	 * the output device device to only select CMYK.
	 */
	if (num_cmyk_used == 4 && pdev->color_info.num_components == 4
	    && pdev->color_info.max_components == 4)
	    return PDF14_DeviceCMYK;
	/*
	 * Check if we should use the 'custom' PDF 1.4 compositor device.
	 * This device is only needed for those devices which do not support
	 * a basic CMYK process color model.
	 */
#if CUSTOM_BLENDING_MODE == AUTO_USE_CUSTOM_BLENDING
	if (num_cmyk != 4)
	    return PDF14_DeviceCustom;
#endif
	/*
	 * Otherewise we use a CMYK plus spot colors for blending.
	 */
	return PDF14_DeviceCMYKspot;
    }
}

/*
 * the PDF 1.4 transparency spec says that color space for blending
 * operations can be based upon either a color space specified in the
 * group or a default value based upon the output device.  We are
 * currently only using a color space based upon the device.
 */
static	int
get_pdf14_device_proto(gx_device * dev, pdf14_device ** pdevproto,
	pdf14_device * ptempdevproto, gs_imager_state * pis,
	const gs_pdf14trans_t * pdf14pct)
{
    pdf14_default_colorspace_t dev_cs =
		pdf14_determine_default_blend_cs(dev);

    switch (dev_cs) {
	case PDF14_DeviceGray:
	    *pdevproto = (pdf14_device *)&gs_pdf14_Gray_device;
	    break;
	case PDF14_DeviceRGB:
	    *pdevproto = (pdf14_device *)&gs_pdf14_RGB_device;
	    break;
	case PDF14_DeviceCMYK:
	    *pdevproto = (pdf14_device *)&gs_pdf14_CMYK_device;
	    break;
	case PDF14_DeviceCMYKspot:
	    *pdevproto = (pdf14_device *)&gs_pdf14_CMYKspot_device;
	    /*
	     * The number of components for the PDF14 device is the sum
	     * of the process components and the number of spot colors
	     * for the page.
	     */
	    if (pdf14pct->params.num_spot_colors >= 0) {
	        *ptempdevproto = **pdevproto;
	        ptempdevproto->devn_params.page_spot_colors =
	    	    pdf14pct->params.num_spot_colors;
	        ptempdevproto->color_info.num_components =
	    	    ptempdevproto->devn_params.num_std_colorant_names +
	    	    pdf14pct->params.num_spot_colors;
	        if (ptempdevproto->color_info.num_components >
	    		ptempdevproto->color_info.max_components)
	    	    ptempdevproto->color_info.num_components =
		        ptempdevproto->color_info.max_components;
		*pdevproto = ptempdevproto;
	    }
	    break;
	case PDF14_DeviceCustom:
	    /*
	     * We are using the output device's process color model.  The
	     * color_info for the PDF 1.4 compositing device needs to match
	     * the output device.
	     */
	    *ptempdevproto = gs_pdf14_custom_device;
	    ptempdevproto->color_info = dev->color_info;
	    *pdevproto = ptempdevproto;
	    break;
	default:			/* Should not occur */
	    return_error(gs_error_rangecheck);
    }
    return 0;
}

/*
 * Recreate the PDF 1.4 compositor device.  Once created, the PDF 1.4
 * compositor device is never removed.  (We do not have a remove compositor
 * method.)  However it is no-op'ed when the PDF 1.4 device is popped.  This
 * routine will re-enable the compositor if the PDF 1.4 device is pushed
 * again.
 */
static	int
pdf14_recreate_device(gs_memory_t *mem,	gs_imager_state	* pis,
		gx_device * dev, const gs_pdf14trans_t * pdf14pct)
{
    pdf14_device * pdev = (pdf14_device *)dev;
    gx_device * target = pdev->target;
    pdf14_device * dev_proto;
    pdf14_device temp_dev_proto;
    int code;

    if_debug0('v', "[v]pdf14_recreate_device\n");

    /*
     * We will not use the entire prototype device but we will set the
     * color related info and the device procs to match the prototype.
     */
    code = get_pdf14_device_proto(target, &dev_proto,
		   		 &temp_dev_proto, pis, pdf14pct);
    if (code < 0)
	return code;
    pdev->color_info = temp_dev_proto.color_info;
    pdev->procs = dev_proto->procs;
    gx_device_fill_in_procs(dev);
    check_device_separable((gx_device *)pdev);

    return code;
}

/*
 * Implement the various operations that can be specified via the PDF 1.4
 * create compositor request.
 */
static	int
gx_update_pdf14_compositor(gx_device * pdev, gs_imager_state * pis,
    const gs_pdf14trans_t * pdf14pct, gs_memory_t * mem )
{
    pdf14_device *p14dev = (pdf14_device *)pdev;
    int code = 0;

    switch (pdf14pct->params.pdf14_op) {
	default:			/* Should not occur. */
	    break;
	case PDF14_PUSH_DEVICE:
	    p14dev->blend_mode = 0;
	    p14dev->opacity = p14dev->shape = 0.0;
	    pdf14_recreate_device(mem, pis, pdev, pdf14pct);
	    break;
	case PDF14_POP_DEVICE:
	    if_debug0('v', "[v]gx_update_pdf14_compositor(PDF14_POP_DEVICE)\n");
	    pis->get_cmap_procs = p14dev->save_get_cmap_procs;
	    gx_set_cmap_procs(pis, p14dev->target);
	    /* Send image out raster data to output device */
	    {	/* hack: Reset lop_pdf14, which could be set by 
		   pdf14_fill_path, pdf14_stroke_path
		   to prevent a failure ingx_image_enum_begin. */
		gs_imager_state new_is = *pis;

		new_is.log_op &= ~lop_pdf14;
		p14dev->pdf14_procs->put_image(pdev, &new_is, p14dev->target);
	    }
	    pdf14_disable_device(pdev);
	    pdf14_close(pdev);
	    break;
	case PDF14_BEGIN_TRANS_GROUP:
	    code = gx_begin_transparency_group(pis, pdev, &pdf14pct->params);
	    break;
	case PDF14_END_TRANS_GROUP:
	    code = gx_end_transparency_group(pis, pdev);
	    break;
	case PDF14_INIT_TRANS_MASK:
	    code = gx_init_transparency_mask(pis, &pdf14pct->params);
	    break;
	case PDF14_BEGIN_TRANS_MASK:
	    code = gx_begin_transparency_mask(pis, pdev, &pdf14pct->params);
	    break;
	case PDF14_END_TRANS_MASK:
	    code = gx_end_transparency_mask(pis, pdev, &pdf14pct->params);
	    break;
	case PDF14_SET_BLEND_PARAMS:
	    pdf14_set_params(pis, pdev, &pdf14pct->params);
	    break;
    }
    return code;
}

/*
 * The PDF 1.4 compositor is never removed.  (We do not have a 'remove
 * compositor' method.  However the compositor is disabled when we are not
 * doing a page which uses PDF 1.4 transparency.  This routine is only active
 * when the PDF 1.4 compositor is 'disabled'.  It checks for reenabling the
 * PDF 1.4 compositor.  Otherwise it simply passes create compositor requests
 * to the targer.
 */
static	int
pdf14_forward_create_compositor(gx_device * dev, gx_device * * pcdev,
	const gs_composite_t * pct, gs_imager_state * pis,
	gs_memory_t * mem)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    gx_device * tdev = pdev->target;
    gx_device * ndev;
    int code = 0;

    *pcdev = dev;
    if (gs_is_pdf14trans_compositor(pct)) {
	const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;

	if (pdf14pct->params.pdf14_op == PDF14_PUSH_DEVICE)
	    return gx_update_pdf14_compositor(dev, pis, pdf14pct, mem);
	return 0;
    }
    code = dev_proc(tdev, create_compositor)(tdev, &ndev, pct, pis, mem);
    if (code < 0)
	return code;
    pdev->target = ndev;
    return 0;
}

/*
 * The PDF 1.4 compositor can be handled directly, so just set *pcdev = dev
 * and return. Since the gs_pdf14_device only supports the high-level routines
 * of the interface, don't bother trying to handle any other compositor.
 */
static	int
pdf14_create_compositor(gx_device * dev, gx_device * * pcdev,
	const gs_composite_t * pct, gs_imager_state * pis,
	gs_memory_t * mem)
{
    if (gs_is_pdf14trans_compositor(pct)) {
	const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;

	*pcdev = dev;
	return gx_update_pdf14_compositor(dev, pis, pdf14pct, mem);
    } else if (gs_is_overprint_compositor(pct)) {
	*pcdev = dev;
	return 0;
    } else
	return gx_no_create_compositor(dev, pcdev, pct, pis, mem);
}

static	int
pdf14_text_begin(gx_device * dev, gs_imager_state * pis,
		 const gs_text_params_t * text, gs_font * font,
		 gx_path * path, const gx_device_color * pdcolor,
		 const gx_clip_path * pcpath, gs_memory_t * memory,
		 gs_text_enum_t ** ppenum)
{
    int code;
    gs_text_enum_t *penum;

    if_debug0('v', "[v]pdf14_text_begin\n");
    pdf14_set_marking_params(dev, pis);
    code = gx_default_text_begin(dev, pis, text, font, path, pdcolor, pcpath,
				 memory, &penum);
    if (code < 0)
	return code;
    *ppenum = (gs_text_enum_t *)penum;
    return code;
}

/*
 * Implement copy_mono by filling lots of small rectangles.
 */
static int
pdf14_copy_mono(gx_device * dev,
	       const byte * base, int sourcex, int sraster, gx_bitmap_id id,
	int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    const byte *sptr;
    const byte *line;
    int sbit, first_bit;
    int code, sbyte, bit, count;
    int run_length, startx, current_bit, bit_value;
    gx_color_index current_color;

    fit_copy(dev, base, sourcex, sraster, id, x, y, w, h);
    line = base + (sourcex >> 3);
    sbit = sourcex & 7;
    first_bit = 7 - sbit;

    /* Loop through the height of the specfied area. */
    while (h-- > 0) {
	/* Set up for the start of each line of the area. */
	sptr = line;
	sbyte = *sptr++;
	bit = first_bit;
	count = w;
        run_length = 0;
	startx = x;
	current_bit = 0;
        current_color = zero;

	/* Loop across each pixel of a line. */
	do {
	    bit_value = (sbyte >> bit) & 1;
	    if (bit_value == current_bit) {
		/* The value did not change, simply increment our run length */
		run_length++;
	    } else {
		/* The value changed, fill the current rectangle. */
		if (run_length != 0) {
		    if (current_color != gx_no_color_index) {
			code = (*dev_proc(dev, fill_rectangle))
				(dev, startx, y, run_length, 1, current_color);
	                if (code < 0)
			    return code;
		    }
		    startx += run_length;
		}
		run_length = 1;
		current_color = bit_value ? one : zero;
		current_bit = bit_value;
	    }
	    /* Move to the next input bit. */
	    if (bit == 0) {
		bit = 7;
		sbyte = *sptr++;
	    }
	    else
		bit--;
	} while (--count > 0);
	/* Fill the last rectangle in the line. */
	if (run_length != 0 && current_color != gx_no_color_index) {
	    code = (*dev_proc(dev, fill_rectangle))
			(dev, startx, y, run_length, 1, current_color);
	    if (code < 0)
		return code;
	}
	/* Move to the next line */
	line += sraster;
	y++;
    }
    return 0;
}

static	int
pdf14_fill_rectangle(gx_device * dev,
		    int x, int y, int w, int h, gx_color_index color)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;

    fit_fill_xywh(dev, x, y, w, h);
    if (w <= 0 || h <= 0)
	return 0;
    if (buf->knockout)
	return pdf14_mark_fill_rectangle_ko_simple(dev, x, y, w, h, color);
    else
	return pdf14_mark_fill_rectangle(dev, x, y, w, h, color);
}

static	int
pdf14_begin_transparency_group(gx_device *dev,
			      const gs_transparency_group_params_t *ptgp,
			      const gs_rect *pbbox,
			      gs_imager_state *pis,
			      gs_transparency_state_t **ppts,
			      gs_memory_t *mem)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    double alpha = pis->opacity.alpha * pis->shape.alpha;
    gs_rect dev_bbox;
    gs_int_rect rect;
    int code;

    code = gs_bbox_transform(pbbox, &ctm_only(pis), &dev_bbox);
    if (code < 0)
	return code;
    rect.p.x = (int)floor(dev_bbox.p.x);
    rect.p.y = (int)floor(dev_bbox.p.y);
    rect.q.x = (int)ceil(dev_bbox.q.x);
    rect.q.y = (int)ceil(dev_bbox.q.y);
    rect_intersect(rect, pdev->ctx->rect);
    /* Make sure the rectangle is not anomalous (q < p) -- see gsrect.h */
    if (rect.q.x < rect.p.x)
	rect.q.x = rect.p.x;
    if (rect.q.y < rect.p.y)
	rect.q.y = rect.p.y;
    if_debug4('v', "[v]begin_transparency_group, I = %d, K = %d, alpha = %g, bm = %d\n",
	      ptgp->Isolated, ptgp->Knockout, alpha, pis->blend_mode);
    code = pdf14_push_transparency_group(pdev->ctx, &rect,
					 ptgp->Isolated, ptgp->Knockout,
					 (byte)floor (255 * alpha + 0.5),
					 (byte)floor (255 * pis->shape.alpha + 0.5),
					 pis->blend_mode);
    return code;
}

static	int
pdf14_end_transparency_group(gx_device *dev,
			      gs_imager_state *pis,
			      gs_transparency_state_t **ppts)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    int code;

    if_debug0('v', "[v]end_transparency_group\n");
    code = pdf14_pop_transparency_group(pdev->ctx, pdev->blend_procs);
    return code;
}

static	int
pdf14_begin_transparency_mask(gx_device	*dev,
			      const gx_transparency_mask_params_t *ptmp,
			      const gs_rect *pbbox,
			      gs_imager_state *pis,
			      gs_transparency_state_t **ppts,
			      gs_memory_t *mem)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    byte bg_alpha = 0;
    byte *transfer_fn = (byte *)gs_alloc_bytes(pdev->ctx->memory, 256,
					       "pdf14_push_transparency_mask");

    if (ptmp->Background_components)
	bg_alpha = (int)(255 * ptmp->GrayBackground + 0.5);
    if_debug1('v', "begin transparency mask, bg_alpha = %d\n", bg_alpha);
    memcpy(transfer_fn, ptmp->transfer_fn, size_of(ptmp->transfer_fn));
    return pdf14_push_transparency_mask(pdev->ctx, &pdev->ctx->rect, bg_alpha,
					transfer_fn);
}

static	int
pdf14_end_transparency_mask(gx_device *dev,
			  gs_transparency_mask_t **pptm)
{
    pdf14_device *pdev = (pdf14_device *)dev;

    if_debug0('v', "end transparency mask!\n");
    return pdf14_pop_transparency_mask(pdev->ctx);
}

static	int
pdf14_mark_fill_rectangle(gx_device * dev,
			 int x, int y, int w, int h, gx_color_index color)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    int i, j, k;
    byte *line, *dst_ptr;
    byte src[PDF14_MAX_PLANES];
    byte dst[PDF14_MAX_PLANES];
    gs_blend_mode_t blend_mode = pdev->blend_mode;
    bool additive = pdev->ctx->additive;
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    bool has_alpha_g = buf->has_alpha_g;
    bool has_shape = buf->has_shape;
    int num_chan = buf->n_chan;
    int num_comp = num_chan - 1;
    int shape_off = num_chan * planestride;
    int alpha_g_off = shape_off + (has_shape ? planestride : 0);
    byte shape = 0; /* Quiet compiler. */
    byte src_alpha;

    if (buf->data == NULL)
	return 0;

    /* NB: gx_color_index is 4 or 8 bytes */
    if (sizeof(color) <= sizeof(ulong))
	if_debug7('v', "[v]pdf14_mark_fill_rectangle, (%d, %d), %d x %d color = %lx  bm %d, nc %d,\n", 
		    x, y, w, h, (ulong)color, blend_mode, num_chan);
    else
	if_debug8('v', "[v]pdf14_mark_fill_rectangle, (%d, %d), %d x %d color = %08lx%08lx  bm %d, nc %d,\n", 
		    x, y, w, h, 
		    (ulong)(color >> 8*(sizeof(color) - sizeof(ulong))), (ulong)color, 
		    blend_mode, num_chan);

    /*
     * Unpack the gx_color_index values.  Complement the components for subtractive
     * color spaces.
     */
    pdev->pdf14_procs->unpack_color(num_comp, color, pdev, src);

    src_alpha = src[num_comp] = (byte)floor (255 * pdev->alpha + 0.5);
    if (has_shape)
	shape = (byte)floor (255 * pdev->shape + 0.5);

    if (x < buf->rect.p.x) x = buf->rect.p.x;
    if (y < buf->rect.p.y) y = buf->rect.p.y;
    if (x + w > buf->rect.q.x) w = buf->rect.q.x - x;
    if (y + h > buf->rect.q.y) h = buf->rect.q.y - y;

    if (x < buf->bbox.p.x) buf->bbox.p.x = x;
    if (y < buf->bbox.p.y) buf->bbox.p.y = y;
    if (x + w > buf->bbox.q.x) buf->bbox.q.x = x + w;
    if (y + h > buf->bbox.q.y) buf->bbox.q.y = y + h;

    line = buf->data + (x - buf->rect.p.x) + (y - buf->rect.p.y) * rowstride;

    for (j = 0; j < h; ++j) {
	dst_ptr = line;
	for (i = 0; i < w; ++i) {
	    /* Complement the components for subtractive color spaces */
	    if (additive) {
		for (k = 0; k < num_chan; ++k)
		    dst[k] = dst_ptr[k * planestride];
	    }
	    else { /* Complement the components for subtractive color spaces */
		for (k = 0; k < num_comp; ++k)
		    dst[k] = 255 - dst_ptr[k * planestride];
		dst[num_comp] = dst_ptr[num_comp * planestride];
	    }
	    art_pdf_composite_pixel_alpha_8(dst, src, num_comp,
			   		 blend_mode, pdev->blend_procs);
	    /* Complement the results for subtractive color spaces */
	    if (additive) {
		for (k = 0; k < num_chan; ++k)
		    dst_ptr[k * planestride] = dst[k];
	    }
	    else {
		for (k = 0; k < num_comp; ++k)
		    dst_ptr[k * planestride] = 255 - dst[k];
		dst_ptr[num_comp * planestride] = dst[num_comp];
	    }
	    if (has_alpha_g) {
		int tmp = (255 - dst_ptr[alpha_g_off]) * (255 - src_alpha) + 0x80;
		dst_ptr[alpha_g_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
	    }
	    if (has_shape) {
		int tmp = (255 - dst_ptr[shape_off]) * (255 - shape) + 0x80;
		dst_ptr[shape_off] = 255 - ((tmp + (tmp >> 8)) >> 8);
	    }
	    ++dst_ptr;
	}
	line += rowstride;
    }
    return 0;
}

static	int
pdf14_mark_fill_rectangle_ko_simple(gx_device *	dev,
				   int x, int y, int w, int h, gx_color_index color)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    pdf14_buf *buf = pdev->ctx->stack;
    int i, j, k;
    byte *line, *dst_ptr;
    byte src[PDF14_MAX_PLANES];
    byte dst[PDF14_MAX_PLANES];
    int rowstride = buf->rowstride;
    int planestride = buf->planestride;
    int num_chan = buf->n_chan;
    int num_comp = num_chan - 1;
    int shape_off = num_chan * planestride;
    bool has_shape = buf->has_shape;
    byte opacity;
    bool additive = pdev->ctx->additive;

    if (buf->data == NULL)
	return 0;

    if (sizeof(color) <= sizeof(ulong))
	if_debug6('v', "[v]pdf14_mark_fill_rectangle_ko_simple, (%d, %d), %d x %d color = %lx, nc %d,\n", 
		    x, y, w, h, (ulong)color, num_chan);
    else
	if_debug7('v', "[v]pdf14_mark_fill_rectangle_ko_simple, (%d, %d), %d x %d color = %8lx%08lx, nc %d,\n", 
		    x, y, w, h, 
		    (ulong)(color >> 8*(sizeof(color) - sizeof(ulong))), (ulong)color, 
		    num_chan);

    /*
     * Unpack the gx_color_index values.  Complement the components for subtractive
     * color spaces.
     */
    pdev->pdf14_procs->unpack_color(num_comp, color, pdev, src);

    src[num_comp] = (byte)floor (255 * pdev->alpha + 0.5);
    opacity = (byte)floor (255 * pdev->opacity + 0.5);

    if (x < buf->rect.p.x) x = buf->rect.p.x;
    if (y < buf->rect.p.y) y = buf->rect.p.y;
    if (x + w > buf->rect.q.x) w = buf->rect.q.x - x;
    if (y + h > buf->rect.q.y) h = buf->rect.q.y - y;

    if (x < buf->bbox.p.x) buf->bbox.p.x = x;
    if (y < buf->bbox.p.y) buf->bbox.p.y = y;
    if (x + w > buf->bbox.q.x) buf->bbox.q.x = x + w;
    if (y + h > buf->bbox.q.y) buf->bbox.q.y = y + h;

    line = buf->data + (x - buf->rect.p.x) + (y - buf->rect.p.y) * rowstride;

    for (j = 0; j < h; ++j) {
	dst_ptr = line;
	for (i = 0; i < w; ++i) {
	    /* Complement the components for subtractive color spaces */
	    if (additive) {
		for (k = 0; k < num_chan; ++k)
		    dst[k] = dst_ptr[k * planestride];
	    }
	    else {
		for (k = 0; k < num_comp; ++k)
		    dst[k] = 255 - dst_ptr[k * planestride];
		dst[num_comp] = dst_ptr[num_comp * planestride];
	    }
	    art_pdf_composite_knockout_simple_8(dst,
		has_shape ? dst_ptr + shape_off : NULL, src, num_comp, opacity);
	    /* Complement the results for subtractive color spaces */
	    if (additive) {
		for (k = 0; k < num_chan; ++k)
		    dst_ptr[k * planestride] = dst[k];
	    }
	    else {
		for (k = 0; k < num_comp; ++k)
		    dst_ptr[k * planestride] = 255 - dst[k];
		dst_ptr[num_comp * planestride] = dst[num_comp];
	    }
	    ++dst_ptr;
	}
	line += rowstride;
    }
    return 0;
}

/**
 * Here we have logic to override the cmap_procs with versions that
 * do not apply the transfer function. These copies should track the
 * versions in gxcmap.c.
 **/

static	cmap_proc_gray(pdf14_cmap_gray_direct);
static	cmap_proc_rgb(pdf14_cmap_rgb_direct);
static	cmap_proc_cmyk(pdf14_cmap_cmyk_direct);
static	cmap_proc_rgb_alpha(pdf14_cmap_rgb_alpha_direct);
static	cmap_proc_separation(pdf14_cmap_separation_direct);
static	cmap_proc_devicen(pdf14_cmap_devicen_direct);
static	cmap_proc_is_halftoned(pdf14_cmap_is_halftoned);

static	const gx_color_map_procs pdf14_cmap_many = {
     pdf14_cmap_gray_direct,
     pdf14_cmap_rgb_direct,
     pdf14_cmap_cmyk_direct,
     pdf14_cmap_rgb_alpha_direct,
     pdf14_cmap_separation_direct,
     pdf14_cmap_devicen_direct,
     pdf14_cmap_is_halftoned
    };

/**
 * Note: copied from gxcmap.c because it's inlined.
 **/
static	inline void
map_components_to_colorants(const frac * pcc,
	const gs_devicen_color_map * pcolor_component_map, frac * plist)
{
    int i = pcolor_component_map->num_colorants - 1;
    int pos;

    /* Clear all output colorants first */
    for (; i >= 0; i--) {
	plist[i] = frac_0;
    }

    /* Map color components into output list */
    for (i = pcolor_component_map->num_components - 1; i >= 0; i--) {
	pos = pcolor_component_map->color_map[i];
	if (pos >= 0)
	    plist[pos] = pcc[i];
    }
}

static	void
pdf14_cmap_gray_direct(frac gray, gx_device_color * pdc, const gs_imager_state * pis,
		 gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    dev_proc(dev, get_color_mapping_procs)(dev)->map_gray(dev, gray, cm_comps);

    for (i = 0; i < ncomps; i++)
	cv[i] = frac2cv(cm_comps[i]);

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
	color_set_pure(pdc, color);
}


static	void
pdf14_cmap_rgb_direct(frac r, frac g, frac b, gx_device_color *	pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    dev_proc(dev, get_color_mapping_procs)(dev)->map_rgb(dev, pis, r, g, b, cm_comps);

    for (i = 0; i < ncomps; i++)
	cv[i] = frac2cv(cm_comps[i]);

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
	color_set_pure(pdc, color);
}

static	void
pdf14_cmap_cmyk_direct(frac c, frac m, frac y, frac k, gx_device_color * pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    dev_proc(dev, get_color_mapping_procs)(dev)->map_cmyk(dev, c, m, y, k, cm_comps);

    for (i = 0; i < ncomps; i++)
	cv[i] = frac2cv(cm_comps[i]);

    color = dev_proc(dev, encode_color)(dev, cv);
    if (color != gx_no_color_index) 
	color_set_pure(pdc, color);
}

static	void
pdf14_cmap_rgb_alpha_direct(frac r, frac g, frac b, frac alpha,	gx_device_color	* pdc,
     const gs_imager_state * pis, gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv_alpha, cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    dev_proc(dev, get_color_mapping_procs)(dev)->map_rgb(dev, pis, r, g, b, cm_comps);

    /* pre-multiply to account for the alpha weighting */
    if (alpha != frac_1) {
#ifdef PREMULTIPLY_TOWARDS_WHITE
	frac alpha_bias = frac_1 - alpha;
#else
	frac alpha_bias = 0;
#endif

	for (i = 0; i < ncomps; i++)
	    cm_comps[i] = (frac)((long)cm_comps[i] * alpha) / frac_1 + alpha_bias;
    }

    for (i = 0; i < ncomps; i++)
	cv[i] = frac2cv(cm_comps[i]);

    /* encode as a color index */
    if (dev_proc(dev, map_rgb_alpha_color) != gx_default_map_rgb_alpha_color &&
	 (cv_alpha = frac2cv(alpha)) != gx_max_color_value)
	color = dev_proc(dev, map_rgb_alpha_color)(dev, cv[0], cv[1], cv[2], cv_alpha);
    else
	color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
	color_set_pure(pdc, color);
}


static	void
pdf14_cmap_separation_direct(frac all, gx_device_color * pdc, const gs_imager_state * pis,
		 gx_device * dev, gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    bool additive = dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE;
    frac comp_value = all;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    if (pis->color_component_map.sep_type == SEP_ALL) {
	/*
	 * Invert the photometric interpretation for additive
	 * color spaces because separations are always subtractive.
	 */
	if (additive)
	    comp_value = frac_1 - comp_value;

	/* Use the "all" value for all components */
	i = pis->color_component_map.num_colorants - 1;
	for (; i >= 0; i--)
	    cm_comps[i] = comp_value;
    }
    else {
	/* map to the color model */
	map_components_to_colorants(&comp_value, &(pis->color_component_map), cm_comps);
    }

    /* apply the transfer function(s); convert to color values */
    if (additive)
	for (i = 0; i < ncomps; i++)
	    cv[i] = frac2cv(gx_map_color_frac(pis,
				cm_comps[i], effective_transfer[i]));
    else
	for (i = 0; i < ncomps; i++)
	    cv[i] = frac2cv(frac_1 - gx_map_color_frac(pis,
			(frac)(frac_1 - cm_comps[i]), effective_transfer[i]));

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
	color_set_pure(pdc, color);
}


static	void
pdf14_cmap_devicen_direct(const	frac * pcc, 
    gx_device_color * pdc, const gs_imager_state * pis, gx_device * dev,
    gs_color_select_t select)
{
    int i, ncomps = dev->color_info.num_components;
    frac cm_comps[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_index color;

    /* map to the color model */
    map_components_to_colorants(pcc, &(pis->color_component_map), cm_comps);;

    /* apply the transfer function(s); convert to color values */
    if (dev->color_info.polarity == GX_CINFO_POLARITY_ADDITIVE)
	for (i = 0; i < ncomps; i++)
	    cv[i] = frac2cv(gx_map_color_frac(pis,
				cm_comps[i], effective_transfer[i]));
    else
	for (i = 0; i < ncomps; i++)
	    cv[i] = frac2cv(frac_1 - gx_map_color_frac(pis,
			(frac)(frac_1 - cm_comps[i]), effective_transfer[i]));

    /* encode as a color index */
    color = dev_proc(dev, encode_color)(dev, cv);

    /* check if the encoding was successful; we presume failure is rare */
    if (color != gx_no_color_index)
	color_set_pure(pdc, color);
}

static	bool
pdf14_cmap_is_halftoned(const gs_imager_state *	pis, gx_device * dev)
{
    return false;
}

static	const gx_color_map_procs *
pdf14_get_cmap_procs(const gs_imager_state *pis, const gx_device * dev)
{
    /* The pdf14 marking device itself is always continuous tone. */
    return &pdf14_cmap_many;
}


int
gs_pdf14_device_push(gs_memory_t *mem, gs_imager_state * pis,
	gx_device ** pdev, gx_device * target, const gs_pdf14trans_t * pdf14pct)
{
    pdf14_device * dev_proto;
    pdf14_device * p14dev, temp_dev_proto;
    int code;

    if_debug0('v', "[v]gs_pdf14_device_push\n");

    code = get_pdf14_device_proto(target, &dev_proto,
		   		 &temp_dev_proto, pis, pdf14pct);
    if (code < 0)
	return code;
    code = gs_copydevice((gx_device **) &p14dev,
			 (const gx_device *) dev_proto, mem);
    if (code < 0)
	return code;

    gs_pdf14_device_copy_params((gx_device *)p14dev, target);

    rc_assign(p14dev->target, target, "gs_pdf14_device_push");

    check_device_separable((gx_device *)p14dev);
    gx_device_fill_in_procs((gx_device *)p14dev);

    p14dev->save_get_cmap_procs = pis->get_cmap_procs;
    pis->get_cmap_procs = pdf14_get_cmap_procs;
    gx_set_cmap_procs(pis, (gx_device *)p14dev);
    code = dev_proc((gx_device *) p14dev, open_device) ((gx_device *) p14dev);
    *pdev = (gx_device *) p14dev;
    pdf14_set_marking_params((gx_device *)p14dev, pis);
    return code;
}

/*
 * In a modest violation of good coding practice, the gs_composite_common
 * fields are "known" to be simple (contain no pointers to garbage
 * collected memory), and we also know the gs_pdf14trans_params_t structure
 * to be simple, so we just create a trivial structure descriptor for the
 * entire gs_pdf14trans_s structure.
 */
#define	private_st_gs_pdf14trans_t()\
  gs_private_st_ptrs1(st_pdf14trans, gs_pdf14trans_t, "gs_pdf14trans_t",\
      st_pdf14trans_enum_ptrs, st_pdf14trans_reloc_ptrs, params.transfer_function)

/* GC descriptor for gs_pdf14trans_t */
private_st_gs_pdf14trans_t();

/*
 * Check for equality of two PDF 1.4 transparency compositor objects.
 *
 * We are currently always indicating that PDF 1.4 transparency compositors are
 * equal.  Two transparency compositors may have teh same data but still
 * represent separate actions.  (E.g. two PDF14_BEGIN_TRANS_GROUP compositor
 * operations in a row mean that we are creating a group inside of a group.
 */
static	bool
c_pdf14trans_equal(const gs_composite_t	* pct0,	const gs_composite_t * pct1)
{
    return false;
}

#ifdef DEBUG
static char * pdf14_opcode_names[] = PDF14_OPCODE_NAMES;
#endif

#define	put_value(dp, value)\
    memcpy(dp, &value, sizeof(value));\
    dp += sizeof(value)

/*
 * Convert a PDF 1.4 transparency compositor to string form for use by the command
 * list device.
 */
static	int
c_pdf14trans_write(const gs_composite_t	* pct, byte * data, uint * psize)
{
    const gs_pdf14trans_params_t * pparams = &((const gs_pdf14trans_t *)pct)->params;
    int need, avail = *psize;
	/* Must be large enough for largest data struct */
    byte buf[21 + sizeof(pparams->Background)
		+ sizeof(pparams->GrayBackground) + sizeof(pparams->bbox)];
    byte * pbuf = buf;
    int opcode = pparams->pdf14_op;
    int mask_size = 0;

    /* Write PDF 1.4 compositor data into the clist */

    *pbuf++ = opcode;			/* 1 byte */
    switch (opcode) {
	default:			/* Should not occur. */
	    break;
	case PDF14_PUSH_DEVICE:
	    put_value(pbuf, pparams->num_spot_colors);	    
	    break;
	case PDF14_POP_DEVICE:
	case PDF14_END_TRANS_GROUP:
	case PDF14_END_TRANS_MASK:
	    break;			/* No data */
	case PDF14_BEGIN_TRANS_GROUP:
	    /*
	     * The bbox data is floating point.  We are not currently using it.
	     * So we are not currently putting it into the clist.  We are also
	     * not using the color space.
	     */
	    *pbuf++ = (pparams->Isolated & 1) + ((pparams->Knockout & 1) << 1);
	    *pbuf++ = pparams->blend_mode;
	    put_value(pbuf, pparams->opacity.alpha);
	    put_value(pbuf, pparams->shape.alpha);
	    put_value(pbuf, pparams->bbox);	    
	    break;
	case PDF14_INIT_TRANS_MASK:
	    *pbuf++ = pparams->csel;
	    break;
	case PDF14_BEGIN_TRANS_MASK:
	    put_value(pbuf, pparams->subtype);
	    *pbuf++ = pparams->function_is_identity;
	    *pbuf++ = pparams->Background_components;
	    if (pparams->Background_components) {
		const int l = sizeof(pparams->Background[0]) * pparams->Background_components;

		memcpy(pbuf, pparams->Background, l);
		pbuf += l;
		memcpy(pbuf, &pparams->GrayBackground, sizeof(pparams->GrayBackground));
		pbuf += sizeof(pparams->GrayBackground);
	    }
	    if (!pparams->function_is_identity)
		mask_size = sizeof(pparams->transfer_fn);
	    break;
	case PDF14_SET_BLEND_PARAMS:
	    *pbuf++ = pparams->changed;
	    if (pparams->changed & PDF14_SET_BLEND_MODE)
		*pbuf++ = pparams->blend_mode;
	    if (pparams->changed & PDF14_SET_TEXT_KNOCKOUT)
		*pbuf++ = pparams->text_knockout;
	    if (pparams->changed & PDF14_SET_OPACITY_ALPHA)
		put_value(pbuf, pparams->opacity.alpha);
	    if (pparams->changed & PDF14_SET_SHAPE_ALPHA)
		put_value(pbuf, pparams->shape.alpha);
	    break;
    }
#undef put_value

    /* check for fit */
    need = (pbuf - buf) + mask_size;
    *psize = need;
    if (need > avail)
	return_error(gs_error_rangecheck);

	/* If we are writing more than the maximum ever expected,
	 * return a rangecheck error.
	 */
	if ( need > MAX_CLIST_COMPOSITOR_SIZE )
		return_error(gs_error_rangecheck);

    /* Copy our serialzed data into the output buffer */
    memcpy(data, buf, need - mask_size);
    if (mask_size)	/* Include the transfer mask data if present */
	memcpy(data + need - mask_size, pparams->transfer_fn, mask_size);
    if_debug2('v', "[v] c_pdf14trans_write: opcode = %s need = %d\n",
				pdf14_opcode_names[opcode], need);
    return 0;
}

/* Function prototypes */
int gs_create_pdf14trans( gs_composite_t ** ppct,
		const gs_pdf14trans_params_t * pparams,
		gs_memory_t * mem );

#define	read_value(dp, value)\
    memcpy(&value, dp, sizeof(value));\
    dp += sizeof(value)

/*
 * Convert the string representation of the PDF 1.4 transparency parameter
 * into the full compositor.
 */
static	int
c_pdf14trans_read(gs_composite_t * * ppct, const byte *	data,
				uint size, gs_memory_t * mem )
{
    gs_pdf14trans_params_t params = {0};
    const byte * start = data;
    int used, code = 0;

    if (size < 1)
	return_error(gs_error_rangecheck);

    /* Read PDF 1.4 compositor data from the clist */
    params.pdf14_op = *data++;
    if_debug2('v', "[v] c_pdf14trans_read: opcode = %s  avail = %d",
				pdf14_opcode_names[params.pdf14_op], size);
    switch (params.pdf14_op) {
	default:			/* Should not occur. */
	    break;
	case PDF14_PUSH_DEVICE:
	    read_value(data, params.num_spot_colors);	    
	    break;
	case PDF14_POP_DEVICE:
	case PDF14_END_TRANS_GROUP:
	    break;			/* No data */
	case PDF14_BEGIN_TRANS_GROUP:
	    /*
	     * We are currently not using the bbox or the colorspace so they were
	     * not placed in the clist
	     */
	    params.Isolated = (*data) & 1;
	    params.Knockout = (*data++ >> 1) & 1;
	    params.blend_mode = *data++;
	    read_value(data, params.opacity.alpha);
	    read_value(data, params.shape.alpha);
	    read_value(data, params.bbox);
	    break;
	case PDF14_INIT_TRANS_MASK:
	    params.csel = *data++;
	    break;
	case PDF14_BEGIN_TRANS_MASK:
		/* This is the largest transparency parameter at this time (potentially
		 * 1275 bytes in size if Background_components = 
		 * GS_CLIENT_COLOR_MAX_COMPONENTS and we have a transfer function
		 * as well).
		 *
		 * NOTE:
		 * The clist reader must be able to handle this sized device.
		 * If any changes are made here the #define MAX_CLIST_COMPOSITOR_SIZE
		 * may also need to be changed correspondingly (defined in gstparam.h)
		 * Also... if another compositor param should exceed this size, this
		 * same condition applies.
		 */
	    read_value(data, params.subtype);
	    params.function_is_identity = *data++;
	    params.Background_components = *data++;
	    if (params.Background_components) {
		const int l = sizeof(params.Background[0]) * params.Background_components;

		memcpy(params.Background, data, l);
		data += l;
		memcpy(&params.GrayBackground, data, sizeof(params.GrayBackground));
		data += sizeof(params.GrayBackground);
	    }
	    if (params.function_is_identity) {
		int i;

		for (i = 0; i < MASK_TRANSFER_FUNCTION_SIZE; i++) {
		    params.transfer_fn[i] = (byte)floor(i *
			(255.0 / (MASK_TRANSFER_FUNCTION_SIZE - 1)) + 0.5);
		}
	    } else {
		read_value(data, params.transfer_fn);
	    }
	    break;
	case PDF14_END_TRANS_MASK:
	    break;			/* No data */
	case PDF14_SET_BLEND_PARAMS:
	    params.changed = *data++;
	    if (params.changed & PDF14_SET_BLEND_MODE)
		params.blend_mode = *data++;
	    if (params.changed & PDF14_SET_TEXT_KNOCKOUT)
		params.text_knockout = *data++;
	    if (params.changed & PDF14_SET_OPACITY_ALPHA)
		read_value(data, params.opacity.alpha);
	    if (params.changed & PDF14_SET_SHAPE_ALPHA)
		read_value(data, params.shape.alpha);
	    break;
    }
    code = gs_create_pdf14trans(ppct, &params, mem);
    if (code < 0)
	return code;
    used = data - start;
    if_debug1('v', "  used = %d\n", used);
    
    /* If we read more than the maximum expected, return a rangecheck error */
    if ( used > MAX_CLIST_COMPOSITOR_SIZE )
 		return_error(gs_error_rangecheck);
    else
    	return used;
}

/*
 * Create a PDF 1.4 transparency compositor.
 *
 * Note that this routine will be called only if the device is not already
 * a PDF 1.4 transparency compositor.
 */
static	int
c_pdf14trans_create_default_compositor(const gs_composite_t * pct,
    gx_device ** pp14dev, gx_device * tdev, gs_imager_state * pis,
    gs_memory_t * mem)
{
    const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;
    gx_device * p14dev = NULL;
    int code = 0;

    /*
     * We only handle the push operation.  All other operations are ignored.
     * The other operations will be handled by the create_compositor routine
     * for the PDF 1.4 compositing device.
     */
    switch (pdf14pct->params.pdf14_op) {
	case PDF14_PUSH_DEVICE:
	    code = gs_pdf14_device_push(mem, pis, &p14dev, tdev, pdf14pct);
	    *pp14dev = p14dev;
	    break;
	default:
	    *pp14dev = tdev;
	    break;
    }
    return code;
}

static	composite_clist_write_update(c_pdf14trans_clist_write_update);
static	composite_clist_read_update(c_pdf14trans_clist_read_update);

/*
 * Methods for the PDF 1.4 transparency compositor
 *
 * Note:  We have two set of methods.  They are the same except for the
 * composite_clist_write_update method.  Once the clist write device is created,
 * we use the second set of procedures.  This prevents the creation of multiple
 * PDF 1.4 clist write compositor devices being chained together.
 */
const gs_composite_type_t   gs_composite_pdf14trans_type = {
    GX_COMPOSITOR_PDF14_TRANS,
    {
	c_pdf14trans_create_default_compositor, /* procs.create_default_compositor */
	c_pdf14trans_equal,                      /* procs.equal */
	c_pdf14trans_write,                      /* procs.write */
	c_pdf14trans_read,                       /* procs.read */
		/* Create a PDF 1.4 clist write device */
	c_pdf14trans_clist_write_update,   /* procs.composite_clist_write_update */
	c_pdf14trans_clist_read_update	   /* procs.composite_clist_reade_update */
    }                                            /* procs */
};

const gs_composite_type_t   gs_composite_pdf14trans_no_clist_writer_type = {
    GX_COMPOSITOR_PDF14_TRANS,
    {
	c_pdf14trans_create_default_compositor, /* procs.create_default_compositor */
	c_pdf14trans_equal,                      /* procs.equal */
	c_pdf14trans_write,                      /* procs.write */
	c_pdf14trans_read,                       /* procs.read */
		/* The PDF 1.4 clist writer already exists, Do not create it. */
	gx_default_composite_clist_write_update, /* procs.composite_clist_write_update */
	c_pdf14trans_clist_read_update	   /* procs.composite_clist_reade_update */
    }                                            /* procs */
};

/*
 * Verify that a compositor data structure is for the PDF 1.4 compositor.
 */
int
gs_is_pdf14trans_compositor(const gs_composite_t * pct)
{
    return (pct->type == &gs_composite_pdf14trans_type
		|| pct->type == &gs_composite_pdf14trans_no_clist_writer_type);
}

/*
 * Create a PDF 1.4 transparency compositor data structure.
 */
int
gs_create_pdf14trans(
    gs_composite_t **               ppct,
    const gs_pdf14trans_params_t *  pparams,
    gs_memory_t *                   mem )
{
    gs_pdf14trans_t *                pct;

    rc_alloc_struct_0( pct,
		       gs_pdf14trans_t,
		       &st_pdf14trans,
		       mem,
		       return_error(gs_error_VMerror),
		       "gs_create_pdf14trans" );
    pct->type = &gs_composite_pdf14trans_type;
    pct->id = gs_next_ids(mem, 1);
    pct->params = *pparams;
    *ppct = (gs_composite_t *)pct;
    return 0;
}

/*
 * Send a PDF 1.4 transparency compositor action to the specified device.
 */
int
send_pdf14trans(gs_imager_state	* pis, gx_device * dev,
    gx_device * * pcdev, gs_pdf14trans_params_t * pparams, gs_memory_t * mem)
{
    gs_composite_t * pct = NULL;
    int code;

    code = gs_create_pdf14trans(&pct, pparams, mem);
    if (code < 0)
	return code;
    code = dev_proc(dev, create_compositor) (dev, pcdev, pct, pis, mem);

    gs_free_object(pis->memory, pct, "send_pdf14trans");

    return code;
}

/* ------------- PDF 1.4 transparency device for clist writing ------------- */

/*
 * The PDF 1.4 transparency compositor device may have a different process
 * color model than the output device.  If we are banding then we need to
 * create two compositor devices.  The output side (clist reader) needs a
 * compositor to actually composite the output.  We also need a compositor
 * device before the clist writer.  This is needed to provide a process color
 * model which matches the PDF 1.4 blending space.
 *
 * This section provides support for this device.
 */

/*
 * Define the default pre-clist (clist writer) PDF 1.4 compositing device.
 * We actually use the same structure for both the clist writer and reader
 * devices.  However we use separate names to identify the routines for each
 * device.
 */

#define	pdf14_clist_procs(get_color_mapping_procs, get_color_comp_index,\
						encode_color, decode_color) \
{\
	NULL,				/* open */\
	gx_forward_get_initial_matrix,	/* get_initial_matrix */\
	gx_forward_sync_output,		/* sync_output */\
	gx_forward_output_page,		/* output_page */\
	gx_forward_close_device,	/* close_device */\
	encode_color,			/* rgb_map_rgb_color */\
	decode_color,			/* map_color_rgb */\
	gx_forward_fill_rectangle,	/* fill_rectangle */\
	gx_forward_tile_rectangle,	/* tile_rectangle */\
	gx_forward_copy_mono,		/* copy_mono */\
	gx_forward_copy_color,		/* copy_color */\
	NULL		,		/* draw_line - obsolete */\
	gx_forward_get_bits,		/* get_bits */\
	gx_forward_get_params,		/* get_params */\
	pdf14_put_params,		/* put_params */\
	encode_color,			/* map_cmyk_color */\
	gx_forward_get_xfont_procs,	/* get_xfont_procs */\
	gx_forward_get_xfont_device,	/* get_xfont_device */\
	NULL,				/* map_rgb_alpha_color */\
	gx_forward_get_page_device,	/* get_page_device */\
	gx_forward_get_alpha_bits,	/* get_alpha_bits */\
	NULL,				/* copy_alpha */\
	gx_forward_get_band,		/* get_band */\
	gx_forward_copy_rop,		/* copy_rop */\
	pdf14_clist_fill_path,		/* fill_path */\
	pdf14_clist_stroke_path,		/* stroke_path */\
	gx_forward_fill_mask,		/* fill_mask */\
	gx_forward_fill_trapezoid,	/* fill_trapezoid */\
	gx_forward_fill_parallelogram,	/* fill_parallelogram */\
	gx_forward_fill_triangle,	/* fill_triangle */\
	gx_forward_draw_thin_line,	/* draw_thin_line */\
	pdf14_clist_begin_image,	/* begin_image */\
	gx_forward_image_data,		/* image_data */\
	gx_forward_end_image,		/* end_image */\
	gx_forward_strip_tile_rectangle, /* strip_tile_rectangle */\
	gx_forward_strip_copy_rop,	/* strip_copy_rop, */\
	gx_forward_get_clipping_box,	/* get_clipping_box */\
	pdf14_clist_begin_typed_image,	/* begin_typed_image */\
	gx_forward_get_bits_rectangle,	/* get_bits_rectangle */\
	NULL,				/* map_color_rgb_alpha */\
	pdf14_clist_create_compositor,	/* create_compositor */\
	gx_forward_get_hardware_params,	/* get_hardware_params */\
	pdf14_clist_text_begin,		/* text_begin */\
	NULL,				/* finish_copydevice */\
	pdf14_begin_transparency_group,\
	pdf14_end_transparency_group,\
	pdf14_begin_transparency_mask,\
	pdf14_end_transparency_mask,\
	NULL,				/* discard_transparency_layer */\
	get_color_mapping_procs,	/* get_color_mapping_procs */\
	get_color_comp_index,		/* get_color_comp_index */\
	encode_color,			/* encode_color */\
	decode_color,			/* decode_color */\
	NULL,				/* pattern_manage */\
	NULL,				/* fill_rectangle_hl_color */\
	NULL,				/* include_color_space */\
	NULL,				/* fill_linear_color_scanline */\
	NULL,				/* fill_linear_color_trapezoid */\
	NULL,				/* fill_linear_color_triangle */\
	gx_forward_update_spot_equivalent_colors	/* update spot */\
}

static	dev_proc_create_compositor(pdf14_clist_create_compositor);
static	dev_proc_create_compositor(pdf14_clist_forward_create_compositor);
static	dev_proc_fill_path(pdf14_clist_fill_path);
static	dev_proc_stroke_path(pdf14_clist_stroke_path);
static	dev_proc_text_begin(pdf14_clist_text_begin);
static	dev_proc_begin_image(pdf14_clist_begin_image);
static	dev_proc_begin_typed_image(pdf14_clist_begin_typed_image);

static	const gx_device_procs pdf14_clist_Gray_procs =
	pdf14_clist_procs(gx_default_DevGray_get_color_mapping_procs,
			gx_default_DevGray_get_color_comp_index,
			gx_default_8bit_map_gray_color,
			gx_default_8bit_map_color_gray);

static	const gx_device_procs pdf14_clist_RGB_procs =
	pdf14_clist_procs(gx_default_DevRGB_get_color_mapping_procs,
			gx_default_DevRGB_get_color_comp_index,
			gx_default_rgb_map_rgb_color,
			gx_default_rgb_map_color_rgb);

static	const gx_device_procs pdf14_clist_CMYK_procs =
	pdf14_clist_procs(gx_default_DevCMYK_get_color_mapping_procs,
			gx_default_DevCMYK_get_color_comp_index,
			cmyk_8bit_map_cmyk_color, cmyk_8bit_map_color_cmyk);

static	const gx_device_procs pdf14_clist_CMYKspot_procs =
	pdf14_clist_procs(pdf14_cmykspot_get_color_mapping_procs,
			pdf14_cmykspot_get_color_comp_index,
			pdf14_compressed_encode_color,
			pdf14_compressed_decode_color);

static	const gx_device_procs pdf14_clist_custom_procs =
	pdf14_clist_procs(gx_forward_get_color_mapping_procs,
			gx_forward_get_color_comp_index,
			gx_forward_encode_color,
			gx_forward_decode_color);

const pdf14_clist_device pdf14_clist_Gray_device = {
    std_device_color_stype_body(pdf14_clist_device, &pdf14_clist_Gray_procs,
			"pdf14clistgray", &st_pdf14_device,
			XSIZE, YSIZE, X_DPI, Y_DPI, 8, 255, 256),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &gray_pdf14_procs,
    &gray_blending_procs
};

const pdf14_clist_device pdf14_clist_RGB_device	= {
    std_device_color_stype_body(pdf14_clist_device, &pdf14_clist_RGB_procs,
			"pdf14clistRGB", &st_pdf14_device,
			XSIZE, YSIZE, X_DPI, Y_DPI, 24, 255, 256),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &rgb_pdf14_procs,
    &rgb_blending_procs
};

const pdf14_clist_device pdf14_clist_CMYK_device = {
    std_device_std_color_full_body_type(pdf14_clist_device,
			&pdf14_clist_CMYK_procs, "PDF14clistcmyk",
			&st_pdf14_device, XSIZE, YSIZE, X_DPI, Y_DPI, 32,
			0, 0, 0, 0, 0, 0),
    { 0 },			/* Procs */
    NULL,			/* target */
    { 0 },			/* devn_params - not used */
    &cmyk_pdf14_procs,
    &cmyk_blending_procs
};

const pdf14_clist_device pdf14_clist_CMYKspot_device = {
    std_device_part1_(pdf14_device, &pdf14_clist_CMYKspot_procs, "PDF14clistcmykspot", &st_pdf14_device, open_init_closed),
    dci_values(GX_DEVICE_COLOR_MAX_COMPONENTS,64,255,255,256,256),
    std_device_part2_(XSIZE, YSIZE, X_DPI, Y_DPI),
    offset_margin_values(0, 0, 0, 0, 0, 0),
    std_device_part3_(),
    { 0 },			/* Procs */
    NULL,			/* target */
    /* DeviceN parameters */
    { 8,			/* Not used - Bits per color */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    &cmykspot_pdf14_procs,
    &cmyk_blending_procs
};

const pdf14_clist_device pdf14_clist_custom_device = {
    std_device_part1_(pdf14_device, &pdf14_clist_CMYKspot_procs, "PDF14clistcustom", &st_pdf14_device, open_init_closed),
    dci_values(GX_DEVICE_COLOR_MAX_COMPONENTS,64,255,255,256,256),
    std_device_part2_(XSIZE, YSIZE, X_DPI, Y_DPI),
    offset_margin_values(0, 0, 0, 0, 0, 0),
    std_device_part3_(),
    { 0 },			/* Procs */
    NULL,			/* target */
    /* DeviceN parameters */
    { 8,			/* Not used - Bits per color */
      DeviceCMYKComponents,	/* Names of color model colorants */
      4,			/* Number colorants for CMYK */
      0,			/* MaxSeparations has not been specified */
      -1,			/* PageSpotColors has not been specified */
      {0},			/* SeparationNames */
      0,			/* SeparationOrder names */
      {0, 1, 2, 3, 4, 5, 6, 7 }	/* Initial component SeparationOrder */
    },
    &custom_pdf14_procs,
    &custom_blending_procs
};


/*
 * the PDF 1.4 transparency spec says that color space for blending
 * operations can be based upon either a color space specified in the
 * group or a default value based upon the output device.  We are
 * currently only using a color space based upon the device.
 */
static	int
get_pdf14_clist_device_proto(gx_device * dev, pdf14_clist_device ** pdevproto,
	pdf14_clist_device * ptempdevproto, gs_imager_state * pis,
	const gs_pdf14trans_t * pdf14pct)
{
    pdf14_default_colorspace_t dev_cs =
		pdf14_determine_default_blend_cs(dev);

    switch (dev_cs) {
	case PDF14_DeviceGray:
	    *pdevproto = (pdf14_clist_device *)&pdf14_clist_Gray_device;
	    break;
	case PDF14_DeviceRGB:
	    *pdevproto = (pdf14_clist_device *)&pdf14_clist_RGB_device;
	    break;
	case PDF14_DeviceCMYK:
	    *pdevproto = (pdf14_clist_device *)&pdf14_clist_CMYK_device;
	    break;
	case PDF14_DeviceCMYKspot:
	    *pdevproto = (pdf14_clist_device *)&pdf14_clist_CMYKspot_device;
	    *pdevproto = (pdf14_clist_device *)&pdf14_clist_custom_device;
	    /*
	     * The number of components for the PDF14 device is the sum
	     * of the process components and the number of spot colors
	     * for the page.
	     */
	    if (pdf14pct->params.num_spot_colors >= 0) {
	        *ptempdevproto = **pdevproto;
	        ptempdevproto->devn_params.page_spot_colors =
	    	    pdf14pct->params.num_spot_colors;
	        ptempdevproto->color_info.num_components =
	    	    ptempdevproto->devn_params.num_std_colorant_names +
	    	    pdf14pct->params.num_spot_colors;
	        if (ptempdevproto->color_info.num_components >
	    		ptempdevproto->color_info.max_components)
	    	    ptempdevproto->color_info.num_components =
	        	ptempdevproto->color_info.max_components;
	        *pdevproto = ptempdevproto;
	    }
	    break;
	case PDF14_DeviceCustom:
	    /*
	     * We are using the output device's process color model.  The
	     * color_info for the PDF 1.4 compositing device needs to match
	     * the output device.
	     */
	    *ptempdevproto = pdf14_clist_custom_device;
	    ptempdevproto->color_info = dev->color_info;
	    *pdevproto = ptempdevproto;
	    break;
	default:			/* Should not occur */
	    return_error(gs_error_rangecheck);
    }
    return 0;
}

static	int
pdf14_create_clist_device(gs_memory_t *mem, gs_imager_state * pis,
				gx_device ** ppdev, gx_device * target,
				const gs_pdf14trans_t * pdf14pct)
{
    pdf14_clist_device * dev_proto;
    pdf14_clist_device * pdev, temp_dev_proto;
    int code;

    if_debug0('v', "[v]pdf14_create_clist_device\n");

    code = get_pdf14_clist_device_proto(target, &dev_proto,
		   		 &temp_dev_proto, pis, pdf14pct);
    if (code < 0)
	return code;
    code = gs_copydevice((gx_device **) &pdev,
			 (const gx_device *) dev_proto, mem);
    if (code < 0)
	return code;

    check_device_separable((gx_device *)pdev);
    gx_device_fill_in_procs((gx_device *)pdev);

    gs_pdf14_device_copy_params((gx_device *)pdev, target);

    rc_assign(pdev->target, target, "pdf14_create_clist_device");

    code = dev_proc((gx_device *) pdev, open_device) ((gx_device *) pdev);
    pdev->pclist_device = target;
    pdev->my_encode_color = pdev->procs.encode_color;
    pdev->my_decode_color = pdev->procs.decode_color;
    pdev->my_get_color_mapping_procs = pdev->procs.get_color_mapping_procs;
    pdev->my_get_color_comp_index = pdev->procs.get_color_comp_index;
    *ppdev = (gx_device *) pdev;
    return code;
}

/*
 * Disable the PDF 1.4 clist compositor device.  Once created, the PDF 1.4
 * compositor device is never removed.  (We do not have a remove compositor
 * method.)  However it is no-op'ed when the PDF 1.4 device is popped.  This
 * routine implements that action.
 */
static	int
pdf14_disable_clist_device(gs_memory_t *mem, gs_imager_state * pis,
				gx_device * dev)
{
    gx_device_forward * pdev = (gx_device_forward *)dev;
    gx_device * target = pdev->target;

    if_debug0('v', "[v]pdf14_disable_clist_device\n");

    /*
     * To disable the action of this device, we forward all device
     * procedures to the target except the create_compositor and copy
     * the target's color_info.
     */
    dev->color_info = target->color_info;
    pdf14_forward_device_procs(dev);
    set_dev_proc(dev, create_compositor, pdf14_clist_forward_create_compositor);
    return 0;
}

/*
 * Recreate the PDF 1.4 clist compositor device.  Once created, the PDF 1.4
 * compositor device is never removed.  (We do not have a remove compositor
 * method.)  However it is no-op'ed when the PDF 1.4 device is popped.  This
 * routine will re-enable the compositor if the PDF 1.4 device is pushed
 * again.
 */
static	int
pdf14_recreate_clist_device(gs_memory_t	*mem, gs_imager_state *	pis,
		gx_device * dev, const gs_pdf14trans_t * pdf14pct)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gx_device * target = pdev->target;
    pdf14_clist_device * dev_proto;
    pdf14_clist_device temp_dev_proto;
    int code;

    if_debug0('v', "[v]pdf14_recreate_clist_device\n");

    /*
     * We will not use the entire prototype device but we will set the
     * color related info to match the prototype.
     */
    code = get_pdf14_clist_device_proto(target, &dev_proto,
		   		 &temp_dev_proto, pis, pdf14pct);
    if (code < 0)
	return code;
    pdev->color_info = dev_proto->color_info;
    pdev->procs = dev_proto->procs;
    gx_device_fill_in_procs(dev);
    check_device_separable((gx_device *)pdev);

    return code;
}

/*
 * Key names are normally C const strings.  However we need to create temp
 * parameter key names.  They only need to have a short life.  We need to
 * create a parameter list with the key names.  Then we will put the parameters
 * into the clist.  That process will create a permanent copy of the key
 * name.  At that point we can release our temp key names.
 */
typedef struct keyname_link_list_s {
	struct keyname_link_list_s * next;
	char * key_name;
    } keyname_link_list_t;

/*
 * The GC description for the keyname link list is being included for
 * completeness.  Since this structure is only temporary, this structure
 * should never be exposed to the GC.
 */
gs_private_st_ptrs2(st_keyname_link_list, keyname_link_list_t,
	       		"keyname_link_list", keyname_link_list_enum_ptrs,
		       	keyname_link_list_reloc_ptrs, next, key_name);

/* See comments before the definition of keyname_link_list_t */
static int
free_temp_keyname_list(gs_memory_t * mem, keyname_link_list_t * plist)
{
    keyname_link_list_t * pthis_elem;

    while (plist != NULL) {
	pthis_elem = plist;
	plist = plist->next;
	gs_free_object(mem, (byte *)pthis_elem, "free_temp_keyname_list");
    }
    return 0;
}

/* Put a data value into our 'string' */
#define put_data(pdata, value, count)\
    for(j = 0; j < count; j++)\
	*pdata++ = (byte)((value) >> (j * 8))

/*
 * Convert a compressed color list element into a set of device parameters.
 * Note:  This routine recursively calls itself.  As a result it can create
 * mulitple device parameters.  The parameters are 'strings'.  Actually the
 * data is stored in the strings as binary data.
 *
 * See comments before the definition of keyname_link_list_t
 */
static int
get_param_compressed_color_list_elem(pdf14_clist_device * pdev,
       	gs_param_list * plist, compressed_color_list_t * pcomp_list,
       	char * keyname, keyname_link_list_t ** pkeyname_list)
{
    int max_list_elem_size =
	    6 + NUM_ENCODE_LIST_ITEMS * sizeof(comp_bit_map_list_t);
    int code, i, j;
    byte * pdata;
    gs_param_string str;

    if (pcomp_list == NULL)	/* Exit if we don not have a list. */
	return 0;

    /* Allocate a string for temp data */
    pdata = gs_alloc_bytes(pdev->memory, max_list_elem_size,
		   		 "convert_compressed_color_list_elem");
    str.data = (const byte *)pdata;
    str.persistent = false;

    put_data(pdata, pcomp_list->num_sub_level_ptrs, 2);
    put_data(pdata, pcomp_list->first_bit_map, 2);

    /* . */
    for (i = pcomp_list->first_bit_map; i < NUM_ENCODE_LIST_ITEMS; i++) {
	put_data(pdata, pcomp_list->u.comp_data[i].num_comp, 2);
	put_data(pdata, pcomp_list->u.comp_data[i].num_non_solid_comp, 2);
	put_data(pdata, pcomp_list->u.comp_data[i].solid_not_100, 1);
        put_data(pdata, pcomp_list->u.comp_data[i].colorants, 
        			sizeof(pcomp_list->u.comp_data[i].colorants));
	if (pcomp_list->u.comp_data[i].num_comp !=
		       	pcomp_list->u.comp_data[i].num_non_solid_comp) {
            put_data(pdata, pcomp_list->u.comp_data[i].solid_colorants, 
        	sizeof(pcomp_list->u.comp_data[i].solid_colorants));
	}
    }
    str.size = pdata - str.data;
    code = param_write_string(plist, keyname, &str);
    gs_free_object(pdev->memory, (byte *)str.data,
		    "convert_compressed_color_list_elem");

    /* Convert the sub levels. */
    for (i = 0; i < pcomp_list->num_sub_level_ptrs; i++) {
	/*
	 * We generate a keyname for the sub level elements based upon
	 * the keyname for the current level.  See comments before the
	 * definition of keyname_link_list_t for comments about the lifetime
	 * of the keynames.
	 */
    	/* Allocate a string for the keyname */
	char * keyname_buf = (char *)gs_alloc_bytes(pdev->memory,
		strlen(keyname) + 10, "convert_compressed_color_list_elem");
	/*
	 * Allocate a link list element so we can keep track of the memory
	 * allocated to hold the keynames.
	 */
       	keyname_link_list_t * pkeyname_list_elem = 
	    gs_alloc_struct(pdev->memory, keyname_link_list_t,
		&st_keyname_link_list, "convert_compressed_color_list_elem");
	pkeyname_list_elem->next = *pkeyname_list;
	pkeyname_list_elem->key_name = keyname_buf;
	*pkeyname_list = pkeyname_list_elem;
	sprintf(keyname_buf, "%s_%d", keyname, i);
	get_param_compressed_color_list_elem(pdev, plist,
		       		pcomp_list->u.sub_level_ptrs[i], keyname_buf,
			       	pkeyname_list);
    }

    return 0;;
}
#undef put_data

/* Get data value from our 'string' */
#define get_data(pdata, value, count)\
    j = count - 1;\
    value = pdata[j--];\
    for(; j >= 0; j--)\
	value = (value <<= 8) | pdata[j];\
    pdata += count

/*
 * Retrieve a compressed color list from a set of device parameters.
 * Note:  This routine recursively calls itself.  As a result it can process
 * mulitple device parameters and create the entire compressed color list.
 * The parameters are 'strings'.  Actually the data is stored in the strings
 * as binary data.
 */
int
put_param_compressed_color_list_elem(gx_device * pdev,
    gs_param_list * plist, compressed_color_list_t ** pret_comp_list,
    char * keyname, int num_comps)
{
    int code, i, j;
    byte * pdata;
    gs_param_string str;
    compressed_color_list_t * pcomp_list;

    /* Check if the given keyname is present. */
    code = param_read_string(plist, keyname, &str);
    switch (code) {
      case 0:
	  break;	/* We have the given keyname, continue. */
      default:
	  param_signal_error(plist, keyname, code);
      case 1:
	  *pret_comp_list = NULL;
	  return 0;
    }
    /* Allocate a compressed color list element. */
    pdata = (byte *)str.data;
    pcomp_list = alloc_compressed_color_list_elem(pdev->memory, num_comps);
    get_data(pdata, pcomp_list->num_sub_level_ptrs, 2);
    get_data(pdata, pcomp_list->first_bit_map, 2);

    /* Read the bit maps */
    for (i = pcomp_list->first_bit_map; i < NUM_ENCODE_LIST_ITEMS; i++) {
	get_data(pdata, pcomp_list->u.comp_data[i].num_comp, 2);
	get_data(pdata, pcomp_list->u.comp_data[i].num_non_solid_comp, 2);
	get_data(pdata, pcomp_list->u.comp_data[i].solid_not_100, 1);
        get_data(pdata, pcomp_list->u.comp_data[i].colorants, 
        			sizeof(pcomp_list->u.comp_data[i].colorants));
	if (pcomp_list->u.comp_data[i].num_comp !=
		       	pcomp_list->u.comp_data[i].num_non_solid_comp) {
            get_data(pdata, pcomp_list->u.comp_data[i].solid_colorants, 
        		sizeof(pcomp_list->u.comp_data[i].solid_colorants));
	}
    }

    /* Get the sub levels. */
    for (i = 0; i < pcomp_list->num_sub_level_ptrs; i++) {
	char buff[50];
        compressed_color_list_t * sub_list_ptr;

	sprintf(buff, "%s_%d", keyname, i);
	put_param_compressed_color_list_elem(pdev, plist,
		       			&sub_list_ptr, buff, num_comps - 1);
	pcomp_list->u.sub_level_ptrs[i] = sub_list_ptr;
    }

    *pret_comp_list = pcomp_list;
    return 0;;
}
#undef get_data

/*
 * Convert a list of spot color names into a set of device parameters.
 * This is done to transfer information from the PDf14 clist writer
 * compositing device to the PDF14 clist reader compositing device.
 *
 * See comments before the definition of keyname_link_list_t
 */
static int
get_param_spot_color_names(pdf14_clist_device * pdev,
       	gs_param_list * plist, keyname_link_list_t ** pkeyname_list)
{
    int code, i;
    gs_param_string str;
    gs_separations * separations = &pdev->devn_params.separations;
    int num_spot_colors = separations->num_separations;

    if (num_spot_colors == 0)
	return 0;

    code = param_write_int(plist, PDF14NumSpotColorsParamName,
		   				 &num_spot_colors);
    for (i = 0; i < num_spot_colors; i++) {
	/*
	 * We generate a keyname for the spot color based upon the
	 * spot color number.  See comments before the definition of
	 * keyname_link_list_t for comments about the lifetime of the keynames.
	 */
    	/* Allocate a string for the keyname */
	char * keyname_buf = (char *)gs_alloc_bytes(pdev->memory,
		strlen("PDF14SpotName_") + 10, "get_param_spot_color_names");
	/*
	 * Allocate a link list element so we can keep track of the memory
	 * allocated to hold the keynames.
	 */
       	keyname_link_list_t * pkeyname_list_elem = 
	    gs_alloc_struct(pdev->memory, keyname_link_list_t,
		&st_keyname_link_list, "get_param_spot_color_names");
	pkeyname_list_elem->next = *pkeyname_list;
	pkeyname_list_elem->key_name = keyname_buf;
	*pkeyname_list = pkeyname_list_elem;
	sprintf(keyname_buf, "%PDF14SpotName_%d", i);
	str.size = separations->names[i].size;
	str.data = separations->names[i].data;
	str.persistent = false;
	code = param_write_string(plist, keyname_buf, &str);
    }
    return 0;;
}

/*
 * Retrieve a list of spot color names for the PDF14 device.
 */
int
put_param_pdf14_spot_names(gx_device * pdev,
	       	gs_separations * pseparations, gs_param_list * plist)
{
    int code, num_spot_colors, i;
    gs_param_string str;

    /* Check if the given keyname is present. */
    code = param_read_int(plist, PDF14NumSpotColorsParamName,
		   				&num_spot_colors);
    switch (code) {
        default:
	    param_signal_error(plist, PDF14NumSpotColorsParamName, code);
	    break;
        case 1:
	    return 0;
        case 0:
	    if (num_spot_colors < 1 ||
		num_spot_colors > GX_DEVICE_COLOR_MAX_COMPONENTS)
		return_error(gs_error_rangecheck);
	    for (i = 0; i < num_spot_colors; i++) {
		char buff[20];
		byte * sep_name;

		sprintf(buff, "PDF14SpotName_%d", i);
		code = param_read_string(plist, buff, &str);
		switch (code) {
	            default:
			param_signal_error(plist, buff, code);
			break;
		    case 0:
			sep_name = gs_alloc_bytes(pdev->memory,
				str.size, "put_param_pdf14_spot_names");
			memcpy(sep_name, str.data, str.size);
			pseparations->names[i].size = str.size;
			pseparations->names[i].data = sep_name;
		}
	    }
	    pseparations->num_separations = num_spot_colors;
	    break;
    }
    return 0;;
}

static int
pdf14_clist_get_param_compressed_color_list(pdf14_device * p14dev)
{
    gx_device_clist_writer * cldev = (gx_device_clist_writer *)p14dev->pclist_device;
    gs_c_param_list param_list;
    keyname_link_list_t * pkeyname_list_head = NULL;
    int code;

    /*
     * If a put_params call fails, the device will be left in a closed
     * state, but higher-level code won't notice this fact.  We flag this by
     * setting permanent_error, which prevents writing to the command list.
     */

    if (cldev->permanent_error)
	return cldev->permanent_error;
    gs_c_param_list_write(&param_list, p14dev->memory);
    code = get_param_compressed_color_list_elem(p14dev,
		(gs_param_list *)&param_list,
		p14dev->devn_params.compressed_color_list,
       		(char *)PDF14CompressedColorListParamName, &pkeyname_list_head);
    get_param_spot_color_names(p14dev, (gs_param_list *)&param_list,
		   	 &pkeyname_list_head);
    if (code >= 0) {
        gx_device * tdev = p14dev->target;

	gs_c_param_list_read(&param_list);
	/* put_param_compressed_color_list_elem(p14dev,
		        (gs_param_list *)&param_list,
		        &pret_comp_list, "PDF14CompressedColorList",
		        TOP_ENCODED_LEVEL);
	 put_param_pdf14_spot_names(p14dev, (gs_param_list *)&param_list); */
#if 1
	code = dev_proc(tdev, put_params)(tdev, (gs_param_list *)&param_list);
#else
	/*
	 * This call will put the compressed color list info into the
	 * clist.  However there are two problems.  The info goes into
	 * the list at the end of the list.
	 */
	code = cmd_put_params(cldev, (gs_param_list *)&param_list );
#endif
    }
    gs_c_param_list_release(&param_list);
    free_temp_keyname_list(p14dev->memory, pkeyname_list_head);

    return code;
}

/*
 * This procedure will have information from the PDF 1.4 clist writing
 * clist compositior device.  This is information output the compressed
 * color list info which is needed for the support of spot colors in
 * PDF 1.4 compositing.  This info needs to be passed to the PDF 1.4
 * clist reading compositor.  However this device is not created until
 * the clist is read.  To get this info to that device, we have to
 * temporarily store that info in the output device.  This routine saves
 * that info in the output device.
 */
int
pdf14_put_devn_params(gx_device * pdev, gs_devn_params * pdevn_params,
	       				gs_param_list * plist)
{
    int code = put_param_compressed_color_list_elem(pdev, plist,
	    &pdevn_params->pdf14_compressed_color_list,
	    (char *)PDF14CompressedColorListParamName, TOP_ENCODED_LEVEL);
    if (code >= 0)
       code = put_param_pdf14_spot_names(pdev,
		       &pdevn_params->pdf14_separations, plist);
    return code;
}

/*
 * When we are banding, we have two PDF 1.4 compositor devices.  One for
 * when we are creating the clist.  The second is for imaging the data from
 * the clist.  This routine is part of the clist writing PDF 1.4 device.
 * This routine is only called once the PDF 1.4 clist write compositor already
 * exists.
 */
static	int
pdf14_clist_create_compositor(gx_device	* dev, gx_device ** pcdev,
    const gs_composite_t * pct, gs_imager_state * pis, gs_memory_t * mem)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    int code;

    /* We only handle a few PDF 1.4 transparency operations 4 */
    if (gs_is_pdf14trans_compositor(pct)) {
	const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;

	switch (pdf14pct->params.pdf14_op) {
	    case PDF14_PUSH_DEVICE:
		/* Re-activate the PDF 1.4 compositor */
		pdev->saved_target_color_info = pdev->target->color_info;
		pdev->target->color_info = pdev->color_info;
		pdev->saved_target_encode_color = pdev->target->procs.encode_color;
		pdev->saved_target_decode_color = pdev->target->procs.decode_color;
		pdev->target->procs.encode_color = pdev->procs.encode_color = pdev->my_encode_color;
		pdev->target->procs.decode_color = pdev->procs.decode_color = pdev->my_decode_color;
		pdev->saved_target_get_color_mapping_procs = pdev->target->procs.get_color_mapping_procs;
		pdev->saved_target_get_color_comp_index = pdev->target->procs.get_color_comp_index;
		pdev->target->procs.get_color_mapping_procs = pdev->procs.get_color_mapping_procs = pdev->my_get_color_mapping_procs;
		pdev->target->procs.get_color_comp_index = pdev->procs.get_color_comp_index = pdev->my_get_color_comp_index;
		pdev->save_get_cmap_procs = pis->get_cmap_procs;
		pis->get_cmap_procs = pdf14_get_cmap_procs;
		gx_set_cmap_procs(pis, dev);
		code = pdf14_recreate_clist_device(mem, pis, dev, pdf14pct);
		pdev->blend_mode = pdev->text_knockout = 0;
		pdev->opacity = pdev->shape = 0.0;
		if (code < 0)
		    return code;
		/*
		 * This routine is part of the PDF 1.4 clist write device.
		 * Change the compositor procs to not create another since we
		 * do not need to create a chain of identical devices.
		 */
		{
		    gs_composite_t pctemp = *pct;

		    pctemp.type = &gs_composite_pdf14trans_no_clist_writer_type;
		    code = dev_proc(pdev->target, create_compositor)
				(pdev->target, pcdev, &pctemp, pis, mem);
		    *pcdev = dev;
		    return code;
		}
	    case PDF14_POP_DEVICE:
		/*
		 * For spot colors we use a 'compressed encoding' for
		 * gx_color_index values.  Send the related data struct
		 * to the clist.
		 */
	        pdf14_clist_get_param_compressed_color_list(pdev);
		/* Restore the color_info for the clist device */
		pdev->target->color_info = pdev->saved_target_color_info;
		pdev->target->procs.encode_color = pdev->saved_target_encode_color;
		pdev->target->procs.decode_color = pdev->saved_target_decode_color;
		pdev->target->procs.get_color_mapping_procs = pdev->saved_target_get_color_mapping_procs;
		pdev->target->procs.get_color_comp_index = pdev->saved_target_get_color_comp_index;
		pis->get_cmap_procs = pdev->save_get_cmap_procs;
		gx_set_cmap_procs(pis, pdev->target);
		/* Disable the PDF 1.4 compositor */
		pdf14_disable_clist_device(mem, pis, dev);
		/*
		 * Make sure that the transfer funtions, etc. are current.
		 */
		code = cmd_put_color_mapping(
			(gx_device_clist_writer *)(pdev->target), pis);
		if (code < 0)
		    return code;
		break;
	    case PDF14_BEGIN_TRANS_GROUP:
		/*
		 * Keep track of any changes made in the blending parameters.
		 */
		pdev->text_knockout = pdf14pct->params.Knockout;
		pdev->blend_mode = pdf14pct->params.blend_mode;
		pdev->opacity = pdf14pct->params.opacity.alpha;
		pdev->shape = pdf14pct->params.shape.alpha;
		{
		    const gs_pdf14trans_params_t * pparams = &((const gs_pdf14trans_t *)pct)->params;

		    if (pparams->Background_components != 0 && 
			pparams->Background_components != pdev->color_info.num_components)
			return_error(gs_error_rangecheck);
		}
		break;
	    default:
		break;		/* Pass remaining ops to target */
	}
    }
    code = dev_proc(pdev->target, create_compositor)
			(pdev->target, pcdev, pct, pis, mem);
    if (*pcdev != pdev->target)
	rc_assign(pdev->target, *pcdev, "pdf14_clist_create_compositor");
    *pcdev = dev;
    return code;
}

/*
 * The PDF 1.4 clist compositor is never removed.  (We do not have a 'remove
 * compositor' method.  However the compositor is disabled when we are not
 * doing a page which uses PDF 1.4 transparency.  This routine is only active
 * when the PDF 1.4 compositor is 'disabled'.  It checks for reenabling the
 * PDF 1.4 compositor.  Otherwise it simply passes create compositor requests
 * to the targer.
 */
static	int
pdf14_clist_forward_create_compositor(gx_device	* dev, gx_device * * pcdev,
	const gs_composite_t * pct, gs_imager_state * pis,
	gs_memory_t * mem)
{
    pdf14_device *pdev = (pdf14_device *)dev;
    gx_device * tdev = pdev->target;
    gx_device * ndev;
    int code = 0;

    *pcdev = dev;
    if (gs_is_pdf14trans_compositor(pct)) {
	const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pct;

	if (pdf14pct->params.pdf14_op == PDF14_PUSH_DEVICE)
	    return pdf14_clist_create_compositor(dev, &ndev, pct, pis, mem);
	return 0;
    }
    code = dev_proc(tdev, create_compositor)(tdev, &ndev, pct, pis, mem);
    if (code < 0)
	return code;
    pdev->target = ndev;
    return 0;
}

/*
 * If any of the PDF 1.4 transparency blending parameters have changed, we
 * need to send them to the PDF 1.4 compositor on the output side of the clist.
 */
static	int
pdf14_clist_update_params(pdf14_clist_device * pdev, const gs_imager_state * pis)
{
    gs_pdf14trans_params_t params = { 0 };
    gx_device * pcdev;
    int changed = 0;
    int code = 0;

    params.pdf14_op = PDF14_SET_BLEND_PARAMS;
    if (pis->blend_mode != pdev->blend_mode) {
	changed |= PDF14_SET_BLEND_MODE;
	params.blend_mode = pdev->blend_mode = pis->blend_mode;
    }
    if (pis->text_knockout != pdev->text_knockout) {
	changed |= PDF14_SET_TEXT_KNOCKOUT;
	params.text_knockout = pdev->text_knockout = pis->text_knockout;
    }
    if (pis->shape.alpha != pdev->shape) {
	changed |= PDF14_SET_SHAPE_ALPHA;
	params.shape.alpha = pdev->shape = pis->shape.alpha;
    }
    if (pis->opacity.alpha != pdev->opacity) {
	changed |= PDF14_SET_OPACITY_ALPHA;
	params.opacity.alpha = pdev->opacity = pis->opacity.alpha;
    }
    /*
     * Put parameters into a compositor parameter and then call the
     * create_compositor.  This will pass the data through the clist
     * to the PDF 1.4 transparency output device.  Note:  This action
     * never creates a new PDF 1.4 compositor and it does not change
     * the imager state.
     */
    if (changed != 0) {
	params.changed = changed;
	code = send_pdf14trans((gs_imager_state *)pis, (gx_device *)pdev,
					&pcdev, &params, pis->memory);
    }
    return code;
}

/*
 * fill_path routine for the PDF 1.4 transaprency compositor device for
 * writing the clist.
 */
static	int
pdf14_clist_fill_path(gx_device	*dev, const gs_imager_state *pis,
			   gx_path *ppath, const gx_fill_params *params,
			   const gx_drawing_color *pdcolor,
			   const gx_clip_path *pcpath)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gs_imager_state new_is = *pis;
    int code;

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the imager state.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pis);
    if (code < 0)
	return code;
    /*
     * The blend operations are not idempotent.  Force non-idempotent
     * filling and stroking operations.
     */
    new_is.log_op |= lop_pdf14;
    return gx_forward_fill_path(dev, &new_is, ppath, params, pdcolor, pcpath);
}

/*
 * stroke_path routine for the PDF 1.4 transaprency compositor device for
 * writing the clist.
 */
static	int
pdf14_clist_stroke_path(gx_device *dev,	const gs_imager_state *pis,
			     gx_path *ppath, const gx_stroke_params *params,
			     const gx_drawing_color *pdcolor,
			     const gx_clip_path *pcpath)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gs_imager_state new_is = *pis;
    int code;

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the imager state.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pis);
    if (code < 0)
	return code;
    /*
     * The blend operations are not idempotent.  Force non-idempotent
     * filling and stroking operations.
     */
    new_is.log_op |= lop_pdf14;
    return gx_forward_stroke_path(dev, &new_is, ppath, params, pdcolor, pcpath);
}

/*
 * text_begin routine for the PDF 1.4 transaprency compositor device for
 * writing the clist.
 */
static	int
pdf14_clist_text_begin(gx_device * dev,	gs_imager_state	* pis,
		 const gs_text_params_t * text, gs_font * font,
		 gx_path * path, const gx_device_color * pdcolor,
		 const gx_clip_path * pcpath, gs_memory_t * memory,
		 gs_text_enum_t ** ppenum)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    gs_text_enum_t *penum;
    int code;

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the imager state.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pis);
    if (code < 0)
	return code;
    /* Pass text_begin to the target */
    code = gx_forward_text_begin(dev, pis, text, font, path,
				pdcolor, pcpath, memory, &penum);
    if (code < 0)
	return code;
    *ppenum = (gs_text_enum_t *)penum;
    return code;
}

static	int
pdf14_clist_begin_image(gx_device * dev,
		       const gs_imager_state * pis, const gs_image_t * pim,
		       gs_image_format_t format, const gs_int_rect * prect,
		       const gx_drawing_color * pdcolor,
		       const gx_clip_path * pcpath,
		       gs_memory_t * memory, gx_image_enum_common_t ** pinfo)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    int code;

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the imager state.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pis);
    if (code < 0)
	return code;
    /* Pass image to the target */
    return gx_forward_begin_image(dev, pis, pim, format, prect,
					pdcolor, pcpath, memory, pinfo);
}

static	int
pdf14_clist_begin_typed_image(gx_device	* dev, const gs_imager_state * pis,
			   const gs_matrix *pmat, const gs_image_common_t *pic,
			   const gs_int_rect * prect,
			   const gx_drawing_color * pdcolor,
			   const gx_clip_path * pcpath, gs_memory_t * mem,
			   gx_image_enum_common_t ** pinfo)
{
    pdf14_clist_device * pdev = (pdf14_clist_device *)dev;
    int code;

    /*
     * Ensure that that the PDF 1.4 reading compositor will have the current
     * blending parameters.  This is needed since the fill_rectangle routines
     * do not have access to the imager state.  Thus we have to pass any
     * changes explictly.
     */
    code = pdf14_clist_update_params(pdev, pis);
    if (code < 0)
	return code;
    /* Pass image to the target */
    return gx_forward_begin_typed_image(dev, pis, pmat,
			    pic, prect, pdcolor, pcpath, mem, pinfo);
}

/*
 * When we push a PDF 1.4 transparency compositor onto the clist, we also need
 * to create a compositing device for clist writing.  The primary purpose of
 * this device is to provide support for the process color model in which
 * the PDF 1.4 transparency is done.  (This may differ from the process color
 * model of the output device.)  The actual work of compositing the image is
 * done on the output (reader) side of the clist.
 */
static	int
c_pdf14trans_clist_write_update(const gs_composite_t * pcte, gx_device * dev,
		gx_device ** pcdev, gs_imager_state * pis, gs_memory_t * mem)
{
    const gs_pdf14trans_t * pdf14pct = (const gs_pdf14trans_t *) pcte;
    pdf14_clist_device * p14dev;
    int code = 0;

    /* We only handle the push/pop operations */
    switch (pdf14pct->params.pdf14_op) {
	case PDF14_PUSH_DEVICE:
	    code = pdf14_create_clist_device(mem, pis, pcdev, dev, pdf14pct);
	    /*
	     * Set the color_info of the clist device to match the compositing
	     * device.  We will restore it when the compositor is popped.
	     * See pdf14_clist_create_compositor for the restore.  Do the same
	     * with the imager state's get_cmap_procs.  We do not want the
	     * imager state to use transfer functions on our color values.  The
	     * transfer functions will be applied at the end after we have done
	     * our PDF 1.4 blend operations.
	     */
	    p14dev = (pdf14_clist_device *)(*pcdev);
	    p14dev->saved_target_color_info = dev->color_info;
	    dev->color_info = (*pcdev)->color_info;
	    p14dev->saved_target_encode_color = dev->procs.encode_color;
	    p14dev->saved_target_decode_color = dev->procs.decode_color;
	    dev->procs.encode_color = p14dev->procs.encode_color = p14dev->my_encode_color;
	    dev->procs.decode_color = p14dev->procs.decode_color = p14dev->my_decode_color;
	    p14dev->saved_target_get_color_mapping_procs = dev->procs.get_color_mapping_procs;
	    p14dev->saved_target_get_color_comp_index = dev->procs.get_color_comp_index;
	    dev->procs.get_color_mapping_procs = p14dev->procs.get_color_mapping_procs = p14dev->my_get_color_mapping_procs;
	    dev->procs.get_color_comp_index = p14dev->procs.get_color_comp_index = p14dev->my_get_color_comp_index;
	    p14dev->save_get_cmap_procs = pis->get_cmap_procs;
	    pis->get_cmap_procs = pdf14_get_cmap_procs;
	    gx_set_cmap_procs(pis, dev);
	    return code;
	case PDF14_POP_DEVICE:
	    /*
	     * Ensure that the tranfer functions, etc.  are current before we
	     * dump our transparency image to the output device.
	     */
            if (pis->dev_ht)
                code = cmd_put_halftone((gx_device_clist_writer *)
                              (((pdf14_clist_device *)dev)->target), pis->dev_ht);
	    break;
	default:
	    break;		/* do nothing for remaining ops */
    }
    *pcdev = dev;
    return code;
}

/*
 * When we push a PDF 1.4 transparency compositor, we need to make the clist
 * device color_info data match the compositing device.  We need to do this
 * since the PDF 1.4 transparency compositing device may use a different
 * process color model than the output device.  We do not need to modify the
 * color related device procs since the compositing device has its own.  We
 * restore the color_info data when the transparency device is popped.
 */
static	int
c_pdf14trans_clist_read_update(gs_composite_t *	pcte, gx_device	* cdev,
		gx_device * tdev, gs_imager_state * pis, gs_memory_t * mem)
{
    pdf14_device * p14dev = (pdf14_device *)tdev;
    gs_pdf14trans_t * pdf14pct = (gs_pdf14trans_t *) pcte;
    gs_devn_params * pclist_devn_params;

    /*
     * We only handle the push/pop operations. Save and restore the color_info
     * field for the clist device.  (This is needed since the process color
     * model of the clist device needs to match the PDF 1.4 compositing
     * device.
     */
    switch (pdf14pct->params.pdf14_op) {
	case PDF14_PUSH_DEVICE:
#	    if 0 /* Disabled because *p14dev has no forwarding methods during the clist playback. 
		    This code is not executed while clist writing. */
	    p14dev->saved_target_color_info = cdev->color_info;
	    cdev->color_info = p14dev->color_info;
	     */
#	    endif
	    /*
	     * If we are blending using spot colors (i.e. the output device
	     * supports spot colors) then we need to transfer compressed
	     * color info from the clist PDF 1.4 compositing reader device
	     * to the clist writer PDF 1.4 compositing device.
	     * This info was transfered from that device to the output
	     * device as a set of device parameters.  However the clist
	     * reader PDF 1.4 compositing device did not exist when the
	     * device parameters were read from the clist.  So that info
	     * was buffered into the output device.
	     */
	    pclist_devn_params = dev_proc(cdev, ret_devn_params)(cdev);
	    if (pclist_devn_params != NULL) {
		int num_comp = p14dev->color_info.num_components;
		/*
		 * The number of components for the PDF14 device is the sum
		 * of the process components and the number of spot colors
		 * for the page.
		 */
		p14dev->devn_params.page_spot_colors =
		    pclist_devn_params->page_spot_colors;
		p14dev->color_info.num_components =
		    p14dev->devn_params.num_std_colorant_names +
		    p14dev->devn_params.page_spot_colors;
		/* Transfer the data for the compressed color encoding. */
		/* free_compressed_color_list(p14dev->memory,
			p14dev->devn_params.compressed_color_list); */
		p14dev->devn_params.compressed_color_list =
		    pclist_devn_params->pdf14_compressed_color_list;
		/* free_separation_names(p14dev->memory,
				 &p14dev->devn_params.separations); */
		p14dev->devn_params.separations =
		    pclist_devn_params->pdf14_separations;
		if ( num_comp != p14dev->color_info.num_components) {
		    dev_proc(tdev, open_device) (tdev);
		    dev_proc(tdev, open_device) (tdev);
		}
	    }
	    break;
	case PDF14_POP_DEVICE:
#	    if 0 /* Disabled because *p14dev has no forwarding methods during the clist playback. 
		    This code is not executed while clist writing. */
	    cdev->color_info = p14dev->saved_target_color_info;
#	    endif
	    break;
	default:
	    break;		/* do nothing for remaining ops */
    }
    return 0;
}

/*
 * This routine will check to see if the color component name matches those
 * that are available amoung the current device's color components.  If the
 * color name is known to the output device then we add it to the list of
 * colorants for the PDF 1.4 transparency compositor. 
 *
 * Notes:  There are currently three different versions of The PDF 1.4
 * transparency compositor device.  The choice of which one is being used
 * depends upon the process color model of the output device.  This procedure
 * is only used if the output (target) device uses a CMYK plus spot color
 * process color model.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns GX_DEVICE_COLOR_MAX_COMPONENTS if
 * the colorant is not being used due to a SeparationOrder device parameter.
 * It returns a negative value if not found.
 */
static int
pdf14_cmykspot_get_color_comp_index(gx_device * dev, const char * pname,
				int name_size, int component_type)
{
    pdf14_device * pdev = (pdf14_device *) dev;
    gx_device * tdev = pdev->target;
    gs_devn_params * pdevn_params = &pdev->devn_params;
    gs_separations * pseparations = &pdevn_params->separations;
    int comp_index;
    dev_proc_get_color_comp_index(*target_get_color_comp_index) = dev_proc(tdev, get_color_comp_index);

    /* The pdf14_clist_create_compositor may have set the color procs. We need the real target procs */
    if (target_get_color_comp_index == pdf14_cmykspot_get_color_comp_index)
	target_get_color_comp_index = ((pdf14_clist_device *)pdev)->saved_target_get_color_comp_index;
    /*
     * If this is not a separation name then simply forward it to the target
     * device.
     */
    if (component_type == NO_COMP_NAME_TYPE)
	return  (*target_get_color_comp_index)(tdev, pname, name_size, component_type);
    /*
     * Check if the component is in either the process color model list
     * or in the SeparationNames list.
     */
    comp_index = check_pcm_and_separation_names(dev, pdevn_params,
					pname, name_size, component_type);
    /*
     * Return the colorant number if we know this name.
     */
    if (comp_index >= 0)
	return comp_index; 
    /*
     * If we do not know this color, check if the output (target) device does.
     */
    comp_index = (*target_get_color_comp_index)(tdev, pname, name_size, component_type);
    /*
     * Ignore color if unknown to the output device or if color is not being
     * imaged due to the SeparationOrder device parameter.
     */
    if (comp_index < 0 || comp_index == GX_DEVICE_COLOR_MAX_COMPONENTS)
	return comp_index; 

    /*
     * This is a new colorant.  Add it to our list of colorants.
     */
    if (pseparations->num_separations < GX_DEVICE_COLOR_MAX_COMPONENTS - 1) {
	int sep_num = pseparations->num_separations++;
        int color_component_number;
	byte * sep_name;

	sep_name = gs_alloc_bytes(dev->memory,
			name_size, "pdf14_cmykspot_get_color_comp_index");
	memcpy(sep_name, pname, name_size);
	pseparations->names[sep_num].size = name_size;
	pseparations->names[sep_num].data = sep_name;
	color_component_number = sep_num + pdevn_params->num_std_colorant_names;
	if (color_component_number >= dev->color_info.num_components)
	    color_component_number = GX_DEVICE_COLOR_MAX_COMPONENTS;
	else
	    pdevn_params->separation_order_map[color_component_number] =
					       color_component_number;
        return color_component_number;
    }

    return GX_DEVICE_COLOR_MAX_COMPONENTS;
}
