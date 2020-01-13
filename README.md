# MOI/MOD video file extractor for Linux

## USE AT YOUR OWN RISK!!!

PLEASE be aware, that this works for me, so I am making it available to
others. However, I have never tested it on anything other than my own
equipment.

## DESCRITION

The Panasonic SDR-H18 digital camcorder (and several other brands) saves
two files for every video, to it's internal hard drive or SD card. It
saves a .MOD file, which is the video file in an MPEG-2 format, and a .MOI
file, which contains information about the video file, such as the date,
frame rate, aspect ratio, etc.

Panasonic provided software for Windows only, which would extract these
files from the camcorder, and do some sort of processing to combine the
.MOD and .MOI files into a single .MPG video file, and save it to the
computer.

Since I run Linux, I needed a way to extract and convert my videos.

This program copies .MOD/.MOI file pairs from the _src_dir_ to the
_dest_dir_, combines them into a single .mpg with the date as the
file name and sets the aspect ratio in the _.mpg_ file.

Note that at the time of this writing, the .MOI file is undocumented. Due
to the hard work of some very generous people, most of it has been
reverse engineered. See reference below.

## BUGS:
 - who knows... so far, all my home videos seem fine.

## TODO:
 - clean up file name stuff - in particular, checking for MOI file by
   open/close. Should instead, check for moi and return `FILE *moi_fp`.
 - Add check to make sure mpeg file size is same as MOD file size
 - Report on when the date contained in the .MOI file is very different
   from the .MOD file creation date.
 - Make block size a parameter.
 - Add ability to extract aspect ratio, frame rate, etc, from mpeg or MOD
 - when looking for .MOI file, may want to try ignoring case for suffix
 - need to implement process_dir() without cd'ing into each dir.  ??? do we?

## REFERENCE

Reference to the MOI file format was found here:
(there are differences in these two descriptions!  Not sure which is right...)
 - http://en.wikipedia.org/wiki/MOI_(file_format)
 - http://forum.camcorderinfo.com/bbs/t135224.html (further down is English translation)

mpeg header reference:  (sequence header is what we care about)
 - http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html

recursive directory traversal:
 - http://code.google.com/p/treetraversal/downloads/detail?name=tree%20traversal.zip&can=2&q=

recursive directory traversal using ftw:
 - http://www.c.happycodings.com/Gnu-Linux/code14.html

## LICENSE

This file is free software. You can redistribute it and/or modify it under the
terms of the FreeBSD License. See header in main `moi.c` file.

