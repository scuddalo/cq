# et:ts=4
# portextract.tcl
# $Id: portextract.tcl 51926 2009-06-06 16:23:50Z jmr@macports.org $
#
# Copyright (c) 2002 - 2003 Apple Computer, Inc.
# Copyright (c) 2007 Markus W. Weissmann <mww@macports.org>
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

package provide portextract 1.0
package require portutil 1.0

set org.macports.extract [target_new org.macports.extract portextract::extract_main]
target_init ${org.macports.extract} portextract::extract_init
target_provides ${org.macports.extract} extract
target_requires ${org.macports.extract} fetch checksum
target_prerun ${org.macports.extract} portextract::extract_start

namespace eval portextract {
}

# define options
options extract.only extract.mkdir
commands extract

# Set up defaults
# XXX call out to code in portutil.tcl XXX
# This cleans the distfiles list of all site tags
default extract.only {[portextract::disttagclean $distfiles]}

default extract.dir {${workpath}}
default extract.cmd {[findBinary gzip ${portutil::autoconf::gzip_path}]}
default extract.pre_args -dc
default extract.post_args {"| ${portutil::autoconf::tar_command} -xf -"}
default extract.mkdir no

set_ui_prefix

# XXX
# Helper function for portextract.tcl that strips all tag names from a list
# Used to clean ${distfiles} for setting the ${extract.only} default
proc portextract::disttagclean {list} {
    if {"$list" == ""} {
        return $list
    }
    foreach name $list {
        lappend val [getdistname $name]
    }
    return $val
}

proc portextract::extract_init {args} {
    global extract.only extract.dir extract.cmd extract.pre_args extract.post_args extract.mkdir distfiles use_bzip2 use_lzma use_zip use_7z use_dmg workpath

    # should the distfiles be extracted to worksrcpath instead?
    if {[tbool extract.mkdir]} {
        global worksrcpath
        ui_debug "Extracting to subdirectory worksrcdir"
        file mkdir ${worksrcpath}
        set extract.dir ${worksrcpath}
    }

    if {[tbool use_bzip2]} {
        option extract.cmd [findBinary bzip2 ${portutil::autoconf::bzip2_path}]
    } elseif {[tbool use_lzma]} {
        option extract.cmd [binaryInPath "lzma"]
    } elseif {[tbool use_zip]} {
        option extract.cmd [findBinary unzip ${portutil::autoconf::unzip_path}]
        option extract.pre_args -q
        option extract.post_args "-d [option extract.dir]"
    } elseif {[tbool use_7z]} {
        option extract.cmd [binaryInPath "7za"]
        option extract.pre_args x
        option extract.post_args ""
    } elseif {[tbool use_dmg]} {
        global worksrcdir
        set dmg_tmp_dir [mkdtemp "/tmp/mports.XXXXXXXX"]
        set dmg_mount ${dmg_tmp_dir}/${worksrcdir}
        file mkdir ${dmg_mount}
        option extract.cmd [findBinary hdiutil ${portutil::autoconf::hdiutil_path}]
        option extract.pre_args attach
        option extract.post_args "-private -readonly -nobrowse -mountpoint ${dmg_mount} && [findBinary cp  ${portutil::autoconf::cp_path}] -Rp ${dmg_mount} ${extract.dir} && ${extract.cmd} detach ${dmg_mount} && [findBinary rmdir  ${portutil::autoconf::rmdir_path}] ${dmg_mount} ${dmg_tmp_dir}"
    }
}

proc portextract::extract_start {args} {
    global UI_PREFIX

    ui_msg "$UI_PREFIX [format [msgcat::mc "Extracting %s"] [option name]]"
}

proc portextract::extract_main {args} {
    global UI_PREFIX filespath worksrcpath extract.dir

    if {![exists distfiles] && ![exists extract.only]} {
        # nothing to do
        return 0
    }

    foreach distfile [option extract.only] {
        ui_info "$UI_PREFIX [format [msgcat::mc "Extracting %s"] $distfile]"
        if {[file exists $filespath/$distfile]} {
            option extract.args "$filespath/$distfile"
        } else {
            option extract.args "[option distpath]/$distfile"
        }
        if {[catch {command_exec extract} result]} {
            return -code error "$result"
        }

    # start gsoc08-privileges
    chownAsRoot ${extract.dir}
    # end gsoc08-privileges

    }
    return 0
}
