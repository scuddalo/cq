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

/* $Id: zfjpx.c 8332 2007-10-30 00:58:44Z giles $ */

/* This is the ps interpreter interface to the JPXDecode filter
   used for (JPEG2000) scanned image compression. PDF only specifies
   a decoder filter, and we don't currently implement anything else. */

#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "gstypes.h"
#include "ialloc.h"
#include "idict.h"
#include "store.h"
#include "stream.h"
#include "strimpl.h"
#include "ifilter.h"
#include "iname.h"
#include "gdebug.h"

#ifdef USE_LWF_JP2
#include "sjpx_luratech.h"
#else
#include "sjpx.h"
#endif

/* <source> /JPXDecode <file> */
/* <source> <dict> /JPXDecode <file> */
static int
z_jpx_decode(i_ctx_t * i_ctx_p)
{
    os_ptr op = osp;
    ref *sop = NULL;
    ref *csname = NULL;
    stream_jpxd_state state;

    /* it's our responsibility to call set_defaults() */
    (*s_jpxd_template.set_defaults)((stream_state *)&state);
    state.jpx_memory = imemory->non_gc_memory;
    if (r_has_type(op, t_dictionary)) {
        check_dict_read(*op);
        if ( dict_find_string(op, "ColorSpace", &sop) > 0) {
	    /* parse the value */
	    if (r_is_array(sop)) {
		/* assume it's the first array element */
		csname =  sop->value.refs;
	    } else if (r_has_type(sop,t_name)) {
		/* use the name directly */
		csname = sop;
	    } else {
		dprintf("warning: JPX ColorSpace value is an unhandled type!\n");
	    }
	    if (csname != NULL) {
		ref sref;
		/* get a reference to the name's string value */
		name_string_ref(imemory, csname, &sref);
		/* request raw index values if the colorspace is /Indexed */
		if (!memcmp(sref.value.const_bytes, "Indexed", min(7,r_size(&sref))))
		    state.colorspace = gs_jpx_cs_indexed;
	    } else {
		if_debug0('w', "[w] Couldn't read JPX ColorSpace key!\n");
	    }
        }
    }
    	
    /* we pass npop=0, since we've no arguments left to consume */
    /* we pass 0 instead of the usual rspace(sop) which will allocate storage
       for filter state from the same memory pool as the stream it's coding.
       this causes no trouble because we maintain no pointers */
    return filter_read(i_ctx_p, 0, &s_jpxd_template,
		       (stream_state *) & state, 0);
}


/* Match the above routine to the corresponding filter name.
   This is how our static routines get called externally. */
const op_def zfjpx_op_defs[] = {
    op_def_begin_filter(),
    {"2JPXDecode", z_jpx_decode},
    op_def_end(0)
};
