#  Copyright 1999-2007 ImageMagick Studio LLC, a non-profit organization
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
#  Makefile for building libltdl.
#
#
AUTOMAKE_OPTIONS = no-dependencies foreign

if WITH_LTDL
  LIBLTDL = ltdl/libltdl.la
if INSTALL_LTDL
  LTDL_INSTALLHEADERS = $(LTDL_HDRS)
else
  LTDL_NOINST_HDRS = $(LTDL_HDRS)
endif # INSTALL_LTDL
else
  LIBLTDL=
endif # WITH_LTDL

LTDL_CLEANFILES = \
	ltdl/libltdl.la

ltdl_libltdl_la_SOURCES = $(LTDL_SRCS)
ltdl_libltdl_la_LDFLAGS = -no-undefined -export-symbols-regex ".*" -version-info 4:5:1
ltdl_libltdl_la_LIBADD = $(LIBADD_DL)

LTDL_SRCS = \
	ltdl/ltdl.c \
	ltdl/ltdl.h

LTDL_HDRS = \
	ltdl/ltdl.h
