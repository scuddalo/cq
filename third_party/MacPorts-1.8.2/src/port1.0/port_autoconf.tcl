# port_autoconf.tcl.in
# $Id: port_autoconf.tcl.in 51958 2009-06-07 10:57:16Z jmr@macports.org $
#
# Copyright (c) 2002 - 2004 Apple Computer, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of Apple Computer, Inc. nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
package provide port 1.0

namespace eval portutil::autoconf {
	variable bzip2_path "/usr/bin/bzip2"
	variable cp_path "/bin/cp"
	variable cpio_path "/usr/bin/cpio"
	variable diff_path "/usr/bin/diff"
	variable ditto_path "/usr/bin/ditto"
	variable dscl_path "/usr/bin/dscl"
	variable file_path "/usr/bin/file"
	variable cvs_path "/usr/bin/cvs"
	variable svn_path "/usr/bin/svn"
	variable git_path "/usr/local/bin/git"
	variable hg_path ""
	variable gzip_path "/usr/bin/gzip"
	variable lipo_path "/usr/bin/lipo"
	variable nice_path "/usr/bin/nice"
	variable patch_path "/usr/bin/patch"
	variable rmdir_path "/bin/rmdir"
	variable rsync_path "/usr/bin/rsync"
	variable unzip_path "/usr/bin/unzip"
	variable zip_path "/usr/bin/zip"
	variable lsbom_path "/usr/bin/lsbom"
	variable make_path "/usr/bin/make"
	variable gnumake_path "/usr/bin/gnumake"
	variable bsdmake_path "/usr/bin/bsdmake"
	variable mkbom_path "/usr/bin/mkbom"
	variable mtree_path "/usr/sbin/mtree"
	variable pax_path "/bin/pax"
	variable xar_path "/usr/bin/xar"
	variable sed_command "/usr/bin/sed"
	variable sed_ext_flag "-E"
	variable tar_command "/usr/bin/gnutar --no-same-owner"
	variable tar_path "/usr/bin/tar"
	variable hdiutil_path "/usr/bin/hdiutil"
	variable have_launchd "yes"
	variable launchctl_path "/bin/launchctl"
	variable install_command "/usr/bin/install -c"
	variable install_user "root"
	variable install_group "admin"
	variable prefix "/opt/local"
	variable macportsuser "root"
}
