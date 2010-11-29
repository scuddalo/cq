#!/usr/bin/perl
#
# Test reading FPX images
#
# Contributed by Bob Friesenhahn <bfriesen@simple.dallas.tx.us>
#

BEGIN { $| = 1; $test=1; print "1..5\n"; }
END {print "not ok $test\n" unless $loaded;}
use Image::Magick;
$loaded=1;

require 't/subroutines.pl';

chdir 't/fpx' || die 'Cd failed';

#
# 1) Test Black-and-white, bit_depth=1 FPX
# 
print( "1-bit grayscale FPX ...\n" );
testRead( 'input_bw.fpx',
  '5088e51ee5060b4d5298b644878bb9d49a34c5e96b25d60dfade19c01b2f95ca' );

#
# 2) Test grayscale FPX
# 
++$test;
print( "8-bit grayscale FPX ...\n" );
testRead( 'input_grayscale.fpx',
  'b64d97d2ad39cca1a8d1163467aa61be9e06de8c93008051cad88b76f66f864b' );

#
# 3) Test 256 color pseudocolor FPX
# 
++$test;
print( "8-bit indexed-color FPX ...\n" );
testRead( 'input_256.fpx',
  'ce5975b3e4b912ddd5a2c49c54f7f25fdbfbf227330fd7e961221c80a0510a96' );

#
# 4) Test TrueColor FPX
# 
++$test;
print( "24-bit Truecolor FPX ...\n" );
testRead( 'input_truecolor.fpx',
  '5a5f94a626ee1945ab1d4d2a621aeec4982cccb94e4d68afe4c784abece91b3e' );

#
# 5) Test JPEG FPX
# 
++$test;
print( "24-bit JPEG FPX ...\n" );
testRead( 'input_jpeg.fpx',
  '7322c1d7332ab1aadf6397e0a929fd1ead4e548e2eb6b042431e7b198ba31089' );

