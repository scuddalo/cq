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

# $Id: gssum.py 7854 2007-04-16 14:45:42Z thomasd $

# gssum.py
#
# this module contains routines for calculating sums and managing
# the sum database

import anydbm
import gsconf
import os, string, md5
from stat import *

myself="gssum.py"

def exists(imagefile, dbname):
    db = anydbm.open(dbname)
    imagefilebase=os.path.basename(imagefile)
    exists = db.has_key(imagefilebase)
    db.close()
    return exists

def add_file(imagefile, dbname, sum=None):
    db = anydbm.open(dbname, 'w')
    if sum == None:
        sum = make_sum(imagefile)
    if sum != None:
        imagefilebase=os.path.basename(imagefile)
        db[imagefilebase] = sum
    else:
        print "gssum.add_file failed to create a sum for",imagefile
    db.close()
    return sum

def get_sum(imagefile, dbname):
    try:
        db = anydbm.open(dbname)
    except:
        print "cannot open", dbname, "for", imagefile
        
    imagefilebase=os.path.basename(imagefile)
    sum = db[imagefilebase]
    db.close()
    return sum

def make_sum(imagefile):
    try:
	mode = os.stat(imagefile)[ST_MODE]
    except OSError:
        print "gssum.add_file failed to stat",imagefile
	return None

    if S_ISREG(mode):
	sum = md5.new()
	f = open(imagefile, "r")
	data = f.read(1024)
	while data:
		sum.update(data)
		data = f.read(1024)
	f.close()

        return sum.hexdigest()
    
    print "gssum.add_file failed ISREG",imagefile
    return None
