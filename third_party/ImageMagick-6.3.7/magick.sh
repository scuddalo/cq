# !/bin/sh
#
#  Copyright 1999-2006 ImageMagick Studio LLC, a non-profit organization
#  dedicated to making software imaging solutions freely available.
#
#  You may not use this file except in compliance with the License.  You may
#  obtain a copy of the License at
#
#    http://www.imagemagick.org/script/license.php
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#  Top-Level Makefile for building ImageMagick.

top_srcdir='/Users/santoash/Projects/cq/third_party/ImageMagick-6.3.7'
top_builddir='/Users/santoash/Projects/cq/third_party/ImageMagick-6.3.7'

MAGICK_CODER_MODULE_PATH='/Users/santoash/Projects/cq/third_party/ImageMagick-6.3.7/coders'
MAGICK_CONFIGURE_SRC_PATH='/Users/santoash/Projects/cq/third_party/ImageMagick-6.3.7/config'
MAGICK_CONFIGURE_BUILD_PATH='/Users/santoash/Projects/cq/third_party/ImageMagick-6.3.7/config'
MAGICK_FILTER_MODULE_PATH='/Users/santoash/Projects/cq/third_party/ImageMagick-6.3.7/filters'
DIRSEP=':'

PATH="${top_builddir}/utilities:${PATH}"

if test -n "$VERBOSE"
then
  echo "$@"
fi
env \
  LD_LIBRARY_PATH="${top_builddir}/magick/.libs:${top_builddir}/wand/.libs:${LD_LIBRARY_PATH}" \
  MAGICK_CODER_MODULE_PATH="${MAGICK_CODER_MODULE_PATH}" \
  MAGICK_CONFIGURE_PATH="${MAGICK_CONFIGURE_BUILD_PATH}${DIRSEP}${MAGICK_CONFIGURE_SRC_PATH}" \
  MAGICK_FILTER_MODULE_PATH="${MAGICK_FILTER_MODULE_PATH}" \
  PATH="${PATH}" \
  "$@"