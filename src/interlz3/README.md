# README #

interlz3 creates an Apple II DOS-ordered (.dsk/.po) disk image file using an Infocom Z-machine interpreter (ZIP)
and Z-code (story file) as input. The Z-code is created using the Inform compiler (6.1.5 creates z3 code).

The problem being solved by interlz3s is that most Infocom's Z-machine interpreters for the Apple II expect
the sectors to be interleaved in a particular order and not just written sequentially to the disk.

This code is based on  work by Steve Nickolas.
The original "Interl" was written in QBASIC for DOS/Windows
and is available at:

http://ifarchive.org/indexes/if-archiveXinfocomXtools.html

Usage:
------
    interlz3 info3m.bin myfile.z3 myfile.dsk

where

   + `info3m.bin` is an example Infocom ZIP
    
   + `myfile.z3` is Z-code generated from the inform compiler
    
   + `myfile.dsk` is the disk image to be created by interlz3


NOTES:
------
To obtain a Z-machine interpreter:

   1. Find an Apple II Infocom disk image having a ZIP (v3) story file (Planetfall).
    
   1. Find the minor version (A, B, ..., M) by playing the game and entering $VERIFY at the parser prompt. 
    
   1. `head --bytes 12288 deadline.dsk > info3a.bin` (where a is the minor version in this example).
    

To create v3 Z-code, use 'inform 6.1.5' and compatible inform libraries.

UPDATES:
--------
2017-10-24: Now handles ZIP (E) interpreter, which is non-interleaved


