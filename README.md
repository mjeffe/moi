# MOI/MOD video file extractor for Linux

## USE AT YOUR OWN RISK!!!

__Disclaimer:__ PLEASE be aware that I have never tested this on anything other
than my own equipment.  It works for me, so I am making it available to others.

**update January 2020** I migrated my personal `Subversion` repository to
github.  I decided to test this code to see if it still works.  Two things: 1)
Yes, it seems to compile and work on my Ubuntu 18.04 laptop, although I had a
little trouble finding the old camcorder :-) and 2) I discovered that I can
simply double click the .MOD file and it will just play. I suspect support for
playing .MOD files has now been added to many newer systems.

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

