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

/* $Id: icfontab.c 8250 2007-09-25 13:31:24Z giles $ */
/* Table of compiled fonts */
#include "ccfont.h"

/*
 * This is compiled separately and linked with the fonts themselves,
 * in a shared library when applicable.
 */

#undef font_
#define font_(fname, fproc, zfproc) extern ccfont_proc(fproc);
#ifndef GCONFIGF_H
# include "gconfigf.h"
#else
# include GCONFIGF_H
#endif

static const ccfont_fproc fprocs[] = {
#undef font_
#define font_(fname, fproc, zfproc) fproc,  /* fname, zfproc are not needed */
#ifndef GCONFIGF_H
# include "gconfigf.h"
#else
# include GCONFIGF_H
#endif
    0
};

int
ccfont_fprocs(int *pnum_fprocs, const ccfont_fproc ** pfprocs)
{
    *pnum_fprocs = countof(fprocs) - 1;
    *pfprocs = &fprocs[0];
    return ccfont_version;	/* for compatibility checking */
}
