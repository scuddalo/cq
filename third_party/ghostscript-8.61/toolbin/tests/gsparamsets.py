#    Copyright (C) 2001 Artifex Software Inc.
# 
# This software is provided AS-IS with no warranty, either express or
# implied.
# 
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
# 
# For more information about licensing, please refer to
# http://www.ghostscript.com/licensing/. For information on
# commercial licensing, go to http://www.artifex.com/licensing/ or
# contact Artifex Software, Inc., 101 Lucas Valley Road #110,
# San Rafael, CA  94903, U.S.A., +1(415)492-9861.

# $Id: gsparamsets.py 7854 2007-04-16 14:45:42Z thomasd $

# gsparamsets.py
#
# Parameter sets for regression testing with gs

class _GSParamSet:
    def __init__(self, device, resolution, banding):
        self.device = device
        self.resolution = resolution
        self.banding = banding
 
testparamsets_name = ""

testparamsets_maximum = [
    # mono
    _GSParamSet('pbmraw', 72, 0),
    _GSParamSet('pbmraw', 72, 1),
    _GSParamSet('pbmraw', 300, 0),
    _GSParamSet('pbmraw', 300, 1),
    _GSParamSet('pbmraw', 600, 0),
    _GSParamSet('pbmraw', 600, 1),

    # grayscale
    _GSParamSet('pgmraw', 72, 0),
    _GSParamSet('pgmraw', 72, 1),
    _GSParamSet('pgmraw', 300, 0),
    _GSParamSet('pgmraw', 300, 1),
    _GSParamSet('pgmraw', 600, 0),
    _GSParamSet('pgmraw', 600, 1),

    # color
    _GSParamSet('ppmraw', 72, 0),
    _GSParamSet('ppmraw', 72, 1),
    _GSParamSet('ppmraw', 300, 0),
    _GSParamSet('ppmraw', 300, 1),
    _GSParamSet('ppmraw', 600, 0),
    _GSParamSet('ppmraw', 600, 1),

    # 1-bit CMYK
    _GSParamSet('pkmraw', 72, 0),
    _GSParamSet('pkmraw', 72, 1),
    _GSParamSet('pkmraw', 300, 0),
    _GSParamSet('pkmraw', 300, 1),
    _GSParamSet('pkmraw', 600, 0),
    _GSParamSet('pkmraw', 600, 1)
]

testparamsets_full = [
    # mono
    _GSParamSet('pbmraw', 72, 0),
    _GSParamSet('pbmraw', 300, 0),
    _GSParamSet('pbmraw', 300, 1),

    # grayscale
    _GSParamSet('pgmraw', 72, 0),
    _GSParamSet('pgmraw', 300, 0),
    _GSParamSet('pgmraw', 300, 1),

    # color
    _GSParamSet('ppmraw', 72, 0),
    _GSParamSet('ppmraw', 300, 0),
    _GSParamSet('ppmraw', 300, 1),

    # 1-bit CMYK
    _GSParamSet('pkmraw', 72, 0),
    _GSParamSet('pkmraw', 300, 0),
    _GSParamSet('pkmraw', 300, 1),
]
testparamsets_minimum = [
    # mono
    _GSParamSet('pbmraw', 72, 0),
    _GSParamSet('pbmraw', 300, 0),
    _GSParamSet('pgmraw', 300, 0),
    _GSParamSet('ppmraw', 300, 0),
]

testparamsets_one = [
    # mono
    _GSParamSet('pbmraw', 300, 0),
]

pdftestparamsets_full = [
    # color
    _GSParamSet('ppmraw', 72, 0),
    _GSParamSet('ppmraw', 300, 0),
    
    # 1-bit CMYK
    _GSParamSet('pkmraw', 300, 0)
]

pdftestparamsets_minimum = [
    # color
    _GSParamSet('ppmraw', 300, 0),
]

pdftestparamsets_maximum = pdftestparamsets_full

testparamsets = testparamsets_minimum
pdftestparamsets = pdftestparamsets_minimum
testparamsets_name = "minimum"

testparamsets = testparamsets_maximum
pdftestparamsets = pdftestparamsets_maximum
testparamsets_name = "maximum"

testparamsets = testparamsets_one
pdftestparamsets = pdftestparamsets_minimum
testparamsets_name = "one"

testparamsets = testparamsets_full
pdftestparamsets = pdftestparamsets_minimum
testparamsets_name = "full"

