# $Id: macports.autoconf.mk.in 54795 2009-08-02 05:19:57Z toby@macports.org $

SHELL			= /bin/sh


srcdir			= .


CC			= gcc
CFLAGS			= -g -O2 $(CFLAGS_QUICHEEATERS) $(CFLAGS_WERROR)
OBJCFLAGS		= -g -O2 $(CFLAGS_QUICHEEATERS) $(CFLAGS_WERROR)
CPPFLAGS		= -I/usr/local/include -DHAVE_CONFIG_H -I.. -I.  -I"/usr/include"
TCL_DEFS		=  -DTCL_THREADS=1 -DUSE_THREAD_ALLOC=1 -D_REENTRANT=1 -D_THREAD_SAFE=1 -DHAVE_PTHREAD_ATTR_SETSTACKSIZE=1 -DHAVE_PTHREAD_ATFORK=1 -DHAVE_READDIR_R=1 -DMAC_OSX_TCL=1 -DHAVE_CFBUNDLE=1 -DUSE_VFORK=1 -DTCL_DEFAULT_ENCODING=\"utf-8\" -DHAVE_GETCWD=1 -DHAVE_OPENDIR=1 -DHAVE_STRSTR=1 -DHAVE_STRTOL=1 -DHAVE_STRTOLL=1 -DHAVE_STRTOULL=1 -DHAVE_TMPNAM=1 -DHAVE_WAITPID=1 -DNO_VALUES_H=1 -DHAVE_LIMITS_H=1 -DHAVE_UNISTD_H=1 -DHAVE_SYS_PARAM_H=1 -DHAVE_SYS_TIME_H=1 -DTIME_WITH_SYS_TIME=1 -DHAVE_TM_ZONE=1 -DHAVE_GMTIME_R=1 -DHAVE_LOCALTIME_R=1 -DHAVE_TM_GMTOFF=1 -DHAVE_TIMEZONE_VAR=1 -DHAVE_ST_BLKSIZE=1 -DSTDC_HEADERS=1 -DHAVE_SIGNED_CHAR=1 -DHAVE_LANGINFO=1 -DHAVE_SYS_IOCTL_H=1 -DHAVE_SYS_FILIO_H=1 -include tclArch.h 
SHLIB_CFLAGS		= -fno-common
CFLAGS_QUICHEEATERS	= -W -Wall -pedantic
CFLAGS_WERROR		= 

READLINE_CFLAGS		=
MD5_CFLAGS		=
SQLITE3_CFLAGS		=
CURL_CFLAGS		= 

OBJC_RUNTIME		= APPLE_RUNTIME
OBJC_RUNTIME_FLAGS	= -fnext-runtime
OBJC_LIBS		= -lobjc

OBJC_FOUNDATION		= Apple
OBJC_FOUNDATION_CPPFLAGS	= 
OBJC_FOUNDATION_LDFLAGS		= 
OBJC_FOUNDATION_LIBS		= -framework Foundation

TCL_CC			= gcc -pipe
SHLIB_LD		= cc -dynamiclib ${LDFLAGS}
STLIB_LD		= ${AR} cr
LDFLAGS			= -L/usr/local/lib
SHLIB_LDFLAGS		=  ${LDFLAGS}
SHLIB_SUFFIX		= .dylib
TCL_STUB_LIB_SPEC	= -L/System/Library/Frameworks/Tcl.framework/Versions/8.4 -ltclstub8.4

LIBS			= 
READLINE_LIBS		= 
MD5_LIBS		= -lcrypto
SQLITE3_LIBS		= -lsqlite3
CURL_LIBS		= -lcurl -lssl -lcrypto -lz
INSTALL			= /usr/bin/install -c
MTREE			= /usr/sbin/mtree
LN_S			= ln -s
XCODEBUILD		= /usr/bin/xcodebuild
BZIP2			= /usr/bin/bzip2

TCLSH			= /usr/bin/tclsh
TCL_PACKAGE_DIR		= /Library/Tcl

DSTUSR			= root
DSTGRP			= admin
DSTMODE			= 0755


prefix			= /opt/local
sysconfdir		= ${prefix}/etc
exec_prefix		= ${prefix}
bindir			= ${exec_prefix}/bin
datarootdir		= ${prefix}/share
datadir			= ${datarootdir}
libdir			= ${exec_prefix}/lib
localstatedir		= ${prefix}/var
infodir			= ${datarootdir}/info

mpconfigdir		= ${sysconfdir}/macports
portsdir		= 

SILENT			= @
