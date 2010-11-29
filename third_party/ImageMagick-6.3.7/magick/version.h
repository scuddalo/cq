/*
  Copyright 1999-2006 ImageMagick Studio LLC, a non-profit organization
  dedicated to making software imaging solutions freely available.
  
  You may not use this file except in compliance with the License.
  obtain a copy of the License at
  
    http://www.imagemagick.org/script/license.php
  
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  MagickCore version methods.
*/
#ifndef _MAGICKCORE_VERSION_H
#define _MAGICKCORE_VERSION_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/*
  Define declarations.
*/
#define MagickPackageName "ImageMagick"
#define MagickCopyright  "Copyright (C) 1999-2007 ImageMagick Studio LLC"
#define MagickLibVersion  0x637
#define MagickLibVersionText  "6.3.7"
#define MagickLibVersionNumber  6,3,7,1
#if (MAGICKCORE_QUANTUM_DEPTH == 8)
#define MagickQuantumDepth  "Q8"
#define MagickQuantumRange  "255"
#elif (MAGICKCORE_QUANTUM_DEPTH == 16)
#define MagickQuantumDepth  "Q16"
#define MagickQuantumRange  "65535"
#elif (MAGICKCORE_QUANTUM_DEPTH == 32)
#define MagickQuantumDepth  "Q32"
#define MagickQuantumRange  "4294967295"
#elif (MAGICKCORE_QUANTUM_DEPTH == 64)
#define MagickQuantumDepth  "Q64"
#define MagickQuantumRange  "18446744073709551615"
#else
#define MagickQuantumDepth  "Q?"
#define MagickQuantumRange  "?"
#endif
#define MagickReleaseDate  "04/10/10"
#if !defined(UseHDRI)
#define MagickVersion MagickPackageName " " MagickLibVersionText " " \
  MagickReleaseDate " " MagickQuantumDepth " http://www.imagemagick.org"
#else
#define MagickVersion MagickPackageName " " MagickLibVersionText " " \
  MagickReleaseDate " " MagickQuantumDepth " HDRI http://www.imagemagick.org"
#endif
#define MagickHomeURL  "file:///usr/local/share/doc/ImageMagick-6.3.7//index.html"

extern MagickExport char
  *GetMagickHomeURL(void);

extern MagickExport const char
  *GetMagickCopyright(void),
  *GetMagickPackageName(void),
  *GetMagickQuantumDepth(unsigned long *),
  *GetMagickQuantumRange(unsigned long *),
  *GetMagickReleaseDate(void),
  *GetMagickVersion(unsigned long *);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
