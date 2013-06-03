/*****************************************************************************
 *
 * $Id$
 *
 * This file is free software. You can redistribute it and/or modify it
 * under the terms of the FreeBSD License which follows.
 * --------------------------------------------------------------------------
 * Copyright (c) 2011, Matt Jeffery (mjeffe@gmail.com). All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * 
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --------------------------------------------------------------------------
 *
 *
 * DESCRITION
 *    My camcorder - Panasonic SDR-H18 saves two files for every video,
 *    A .MOD file, which is the video file in an MPEG-2 format, and a
 *    .MOI file, which contains information about the video file, such
 *    as the date, frame rate, aspect ratio, etc.
 *
 *    This program copies .MOD/.MOI file pairs from the src_dir to the
 *    dest_dir, combines them into a single .mpg with the date as the
 *    file name and sets the aspect ratio in the .mpg file.
 *
 * TODO:
 *    - look at making some of the path variables static (i.e. not malloc'ed)
 *    - clean up file name stuff - in particular, checking for MOI file by open/close. Should
 *      instead, check for moi and return FILE *moi_fp.
 *    - Add ability to use MOI date, but make unique file name if duplicate - moi resolution is 1 minute!
 *    - Add check to make sure mpeg file size is same as MOD file size
 *    - modify so we can get info only on an MOI file, regardless of whether or not
 *      it has an accompanying MOD file. This will require rearanging how process_file()
 *      and process_mod() work.
 *    - Report on when the date contained in the .MOI file is very different
 *      from the .MOD file creation date.
 *    - Make block size a parameter.
 *    - Add ability to extract aspect ratio, frame rate, etc, from mpeg or MOD
 *    - when looking for .MOI file, may want to try ignoring case for suffix
 *
 * AUTHOR
 *    Matt Jeffery (mjeffe@gmail.com)
 *
 * REFERENCE
 *    Reference to the MOI file format was found here:
 *    http://en.wikipedia.org/wiki/MOI_(file_format)
 *    http://forum.camcorderinfo.com/bbs/t135224.html   (further down is English translation)
 *    NOTE: there are differences in those two descriptions!  Not sure which is right...
 *
 *    mpeg header reference:  (sequence header is what we care about)
 *    http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html
 *
 *    recursive directory traversal:
 *    http://www.dreamincode.net/code/snippet271.htm
 *    http://code.google.com/p/treetraversal/downloads/detail?name=tree%20traversal.zip&can=2&q=
 *
 *    recursive directory traversal using ftw:
 *    http://www.c.happycodings.com/Gnu-Linux/code14.html
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>    /* strcpy, strdup, etc */
#include <sys/types.h> /* stat, mkdir */
#include <sys/stat.h>  /* stat, mkdir */
#include <errno.h>
#include <unistd.h>    /* stat, NULL, etc */
#include <time.h>      /* localtime */
#include <dirent.h>    /* opendir, chdir, getcwd, etc */
#include <libgen.h>    /* basename, dirname */
#include <getopt.h>    /* getopt_long */
//#include <ftw.h>       /* recursive directory traversal - not portable... */
//#include <fcntl.h>     /* old, lower level file stuff - open, read, etc */


#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define MAX_PATH_LEN 2048        /* max length for a path/filename string */
#define RW_BLOCK_SIZE 1048576    /* read 1MB chunks at a time */
#define MTIME_DATE 1
#define MOI_DATE   2

//typedef struct stat Stat;


typedef struct moi_info {
   /* just store everything. This will make it more flexible for future use */
   unsigned char aspect_ratio;         /* binary value found in MOI */
   char          aspect_ratio_str[10]; /* string representation - "4:3" or "16:9" */
   unsigned short moi_year;
   unsigned char moi_mon;
   unsigned char moi_day;
   unsigned char moi_hour;
   unsigned char moi_min;
   char          moi_date_str[64]; /* date string like: 20101004-2024 or 2010-10-04-2014 */
   time_t        mtime;
   int           mtime_year;
   int           mtime_mon;
   int           mtime_day;
   int           mtime_hour;
   int           mtime_min;
   int           mtime_sec;
   char          mtime_date_str[64]; /* date string like: 20101004-2024 or 2010-10-04-2014 */
} moi_info_type;


/*
 * globals
 */
char *this;
int verbose = 0;
char *dest_dir;
int make_dirs = 1;           /* if set, create seperate directories for each date */
int recursive = 0;           /* if set, process subdirs */
int date_to_use = MOI_DATE;  /* default to using date in MOI file */
int info_only = 0;           /* if set, don't copy, only report MOI info */
int noclobber = 1;           /* if set, do not overwrite existing mpeg files */
static char *mpeg_seqh_ar_codes[] = {   /* mpeg sequence header aspect ratio codes */
   "forbidden!",
   "1:1",
   "4:3",
   "16:9",
   "2.21:1",
   "reserved","reserved","reserved","reserved","reserved","reserved",
   "reserved","reserved","reserved","reserved","reserved"
}; /* 5-15 reserved */
static char *mpeg_seqh_fr_codes[] = {   /* mpeg sequence header frame rate codes */
   "forbidden!",
   "24000/1001 (23.976)",
   "24",
   "25",                    /* PAL */
   "30000/1001 (29.97)",    /* NTSC */
   "30",
   "50",
   "60000/1001 (59.94)",
   "60",
   "reserved","reserved","reserved","reserved","reserved","reserved","reserved"
}; /* 9-15 reserved */


/*
 * function prototypes
 */
void usage();
int locate_moi(char *moi_fname, char *mod_fname);
int get_moi_info(moi_info_type *info, char *mod_fname);
static int chomp(char *s);
static int isdir(char *name);
int ignore_ent(char *name);
//int is_search_ent(char *fname);
int is_file_type(char *fname, char **suffixes);
void process_dir(char *dirname);
void process_file(char *dir, char *fname);
void process_mod(char *dir, char *fname);
void make_mpeg(char *mod_fname, char *output_dir, moi_info_type *info);
void copy_moi(char *moi_fname, char *output_dir, moi_info_type *info);
int set_mpeg_ar(FILE *mpeg, char *moi_ar_str);
static int do_mkdir(const char *path, mode_t mode);
int mkpath(const char *path, mode_t mode);



/*****************************************************************************
 * MAIN
 ****************************************************************************/
int main(int argc, char *argv[]) {
   int c;
   char *src_dir = NULL;
   char *src_file = NULL, *src_file_base = NULL, *src_file_cpy1 = NULL, *src_file_cpy2 = NULL;
   char abs_dest_dir[MAX_PATH_LEN];  /* used to make dest_dir an absolute path */
   char cwd[MAX_PATH_LEN];
   DIR *dir;
   /* getopt_long structures */
   int option_index = 0;
   static struct option long_options[] = {
      {"verbose",          no_argument,       0, 'v'},
      {"help",             no_argument,       0, 'h'},
      {"info",             no_argument,       0, 'i'},
      {"clobber",          no_argument,       0, 'c'},
      {"modification-time",no_argument,       0, 't'},
      {"mtime",            no_argument,       0, 't'},  /* alias */
      {"make-dirs",        no_argument,       0, 'm'},  /* c for create dirs */
      {"recursive",        no_argument,       0, 'r'},
      {"mod-file",         required_argument, 0, 'f'},
      {"src-dir",          required_argument, 0, 's'},
      {"dest-dir",         required_argument, 0, 'd'},
      {0, 0, 0, 0}
   };


   this = argv[0];


   optarg = NULL;
   //while ((c = getopt_long(argc, argv, "itvho", long_options, &option_index)) != -1 ) {
   while (1) {
      c = getopt_long(argc, argv, "vhictmrf:s:d:", long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1)
        break;

      switch(c) {
         case 0:
            /* If this option set a flag, do nothing else now. */
            if (long_options[option_index].flag != 0)
               break;
            /*
            printf ("option %s", long_options[option_index].name);
            if (optarg)
               printf (" with arg %s", optarg);
            printf ("\n");
            break;
            */

         /* do not overwrite existing files */
         case 'c':
            noclobber = 0;
            break;
         /* print MOI info only, don't copy or create mpeg */
         case 'i':
            info_only++;
            break;
         /* use mtime date rather than moi date */
         case 't':
            date_to_use = MTIME_DATE;
            break;
         /* do NOT create directory for each date */
         case 'm':
            make_dirs = 0;
            break;
         /* process a single MOD file, rather than an entire directory */
         case 'f':
            src_file = optarg;
            break;
         case 's':
            src_dir = optarg;
            break;
         case 'd':
            dest_dir = optarg;
            break;
         case 'r':
            recursive = 1;
            break;
         case 'v':
            verbose++;
            break;
         case 'h':
            usage();
            exit(1);
         default:
            //fprintf(stderr,"%s: Unknown or invalid parameters\n", this);
            exit(1);
      }
   }

   /*
   if ( (argc == optind + 1) ) {
      src_dir = argv[optind];
   }
   else if ( argc != optind + 2 ) {
      src_dir = argv[optind];
      dest_dir = argv[optind+1];
   }
   */


   /*
    * Validate arguments and options
    */

   if ( (argc > optind) ) {
      fprintf(stderr, "Error: unknown options or extra stuff on the command line.\n");
      exit(1);
   }

   /* help, version, etc. should have been taken care of above,
    * for anything else we need an input source */
   if ( !(src_dir || src_file) ) {
      fprintf(stderr, "%s: Error: missing input source option - either -s or -f\n", this);
      exit(1);
   }

   /* unless info_only, we also need an output dir */
   if ( !info_only && !dest_dir ) {
      fprintf(stderr, "%s: Error: missing output source option -d\n", this);
      exit(1);
   } 

   /* if asking for info only, we need an MOI file */
   /*
   if ( info_only && src_file ) {
      if ( strcasecmp((src_file + strlen(src_file) - 4), ".MOI" ) != 0 ) {
         fprintf(stderr, "%s: ERROR, when using -i, you need to specify an MOI file with -f\n", this);
         exit(1);
      }
   }
   */

   /*
    * Do the work
    */

   /* strip trailing / if any from dest_dir */
   if ( !info_only ) {
      if ( dest_dir && dest_dir[strlen(dest_dir)-1] == '/' )
         dest_dir[strlen(dest_dir)-1] = '\0';

      /* turn dest_dir into an absolute path */
      if ( dest_dir[0] != '/' ) {
         if ( !getcwd(cwd, sizeof cwd) ) {
            perror("cannot get cwd\n");
            exit(1);
         }
         sprintf(abs_dest_dir, "%s/%s", cwd, dest_dir);
         dest_dir = abs_dest_dir;
      }

      /* check to see if dir exists.
       * note, in general this is not a great idea to check dir, close it, then use it later
       * since it can cause an obvious race condition... */
      dir = opendir(dest_dir);
      if ( dir ) {
         closedir(dir);  /* dir exists */
      } else {
         fprintf(stderr, "%s: destination directory does not exist: %s\n", this, dest_dir);
         exit(1);
      }
   }

   if ( src_file ) {
      /* man page says dirname/basename may clobber string, so make copies */
      src_file_cpy1 = strdup(src_file);
      src_file_cpy2 = strdup(src_file);
      src_dir = dirname(src_file_cpy2);
      src_file_base = basename(src_file_cpy1);
      process_file(src_dir, src_file_base);
   }
   else {
      process_dir(src_dir);
   }

   return(0);
}


/*****************************************************************************
 * call process_file() on every file in dirname. 
 * If -r, recursively descend into directories except . and ..
 ****************************************************************************/
void process_dir(char *dirname) {
   DIR *dir;
   struct dirent *ent;
   char cwd[MAX_PATH_LEN];
   char newd[MAX_PATH_LEN];

   if ( !getcwd(cwd, sizeof cwd) ) {
      perror("cannot get cwd\n");
      exit(1);
   }
   if ( chdir(dirname) < 0 ) {
      perror(dirname);
      exit(1);
   }
   if ( !getcwd(newd, sizeof newd) ) {
      perror("cannot get cwd for new dir\n");
      exit(1);
   }
   if ( verbose >= 2 )
      printf("%s: processing %s\n", this, newd);

   if ( !(dir = opendir(".")) ) {
      perror(dirname);
      exit(1);
   }
 
   while ( (ent = readdir(dir)) ) {
      if ( ignore_ent(ent->d_name) )
         continue;
      else if ( isdir(ent->d_name) && recursive ) {
         process_dir(ent->d_name);
      }
      else
         process_file(newd, ent->d_name);
   }
   closedir(dir);
   if ( chdir(cwd) < 0 ) {
      perror(cwd);
      exit(1);
   }
}


/*****************************************************************************
 * Function to be called from process_dir - trying to keep process_dir()
 * a generic directory traversal.
 ****************************************************************************/
void process_file(char *dir, char *fname) {
   static char *mod_suffix[] = { ".mod", NULL };
   if ( is_file_type(fname, mod_suffix) )
      process_mod(dir, fname); 
}


/*****************************************************************************
 * 1) check for sister MOI file
 * 2) extract necessary info from MOI file
 * 3) create mpeg file in target dir
 ****************************************************************************/
void process_mod(char *dir, char *fname) {
   char *mod_fname, *moi_fname, *mpeg_dirname;
   moi_info_type *info;


   info = (moi_info_type *) malloc(sizeof(moi_info_type));
   moi_fname = (char *) malloc(MAX_PATH_LEN);
   mod_fname = (char *) malloc(MAX_PATH_LEN);
   mpeg_dirname   = (char *) malloc(MAX_PATH_LEN);
   if ( info == NULL || mod_fname == NULL || moi_fname == NULL || mpeg_dirname == NULL ) {
      fprintf(stderr, "%s: unable to allocate memory in process_mod()\n", this);
      exit(1);
   }

   sprintf(mod_fname, "%s/%s", dir, fname);
   if ( verbose >= 2 )
      printf("-----------------------------------\n");
   if ( verbose >= 1 )
      printf("%s: processing %s\n", this, mod_fname);

   if ( ! locate_moi(moi_fname, mod_fname) ) {
      fprintf(stderr, "%s: WARNING: no matching .MOI file for %s\n", this, mod_fname);
      fprintf(stderr, "   skipping...\n");
      return;
   }

   if ( ! get_moi_info(info, moi_fname) || info_only )
      return;

   if ( make_dirs ) {
      /* build output dir structure */
      if ( date_to_use == MTIME_DATE )
         sprintf(mpeg_dirname, "%s/%04d/%02d/%02d\0", dest_dir, info->mtime_year, info->mtime_mon, info->mtime_day);
      else
         sprintf(mpeg_dirname, "%s/%04d/%02d/%02d\0", dest_dir, info->moi_year, info->moi_mon, info->moi_day);

      if ( verbose >= 3 )
         printf("%s: creating target dir %s\n", this, mpeg_dirname);
      mkpath(mpeg_dirname, 0777);
   }
   else {
      sprintf(mpeg_dirname, "%s\0", dest_dir);
   }

   make_mpeg(mod_fname, mpeg_dirname, info);
   copy_moi(moi_fname, mpeg_dirname, info);

   free(info);
   free(moi_fname);
   free(mod_fname);
   free(mpeg_dirname);
}

/*****************************************************************************
 ****************************************************************************/
void copy_moi(char *moi_fname, char *output_dir, moi_info_type *info) {
   FILE *src, *dest;
   char *buf, *dest_fname;
   int br=0, bw=0;

   buf = (char *) malloc(RW_BLOCK_SIZE);
   dest_fname = (char *) malloc(MAX_PATH_LEN);
   if (buf == NULL || dest_fname == NULL ) {
      fprintf(stderr, "%s: unable to allocate memory for buffers in copy_moi()\n", this);
      exit(1);
   }


   /* build the moi file name */
   if ( date_to_use == MTIME_DATE )
      sprintf(dest_fname, "%s/mov-%s.moi", output_dir, info->mtime_date_str);
   else 
      sprintf(dest_fname, "%s/mov-%s.moi", output_dir, info->moi_date_str);

   if ( verbose >= 2 )
      fprintf(stderr, "%s: copying moi file\n", this);

   /* test to see if the file already exists */
   if ( noclobber ) {
      if (dest = fopen(dest_fname, "rb")) {
         fclose(dest);
         if ( verbose >= 2 )
            fprintf(stderr, "   skipping... file exists and noclobber is on\n");
         free(buf);
         return;
      }
   }

   /* open copy from file */
   if ( (src = fopen(moi_fname, "rb")) == NULL ) {
      fprintf(stderr, "%s: WARNING: cannot open .MOI file to copy %s\n", this, moi_fname);
      perror(moi_fname);
      fprintf(stderr, "   skipping...\n");
      return;
   }
   /* open copy to file */
   if ( (dest = fopen(dest_fname, "wb")) == NULL ) {
      fprintf(stderr, "%s: unable to open %s\n", this, dest_fname);
      perror(dest_fname);
      fclose(src);
      return;
   }

   /* copy data from mod file to the mpeg file */
   while( (br = fread(buf, 1, RW_BLOCK_SIZE, src)) > 0 ) {
      bw = fwrite(buf, 1, br, dest);
      if ( bw < 0 || ferror(dest) ) {
         perror("write failed");
         exit(1);
      }
   }

   fclose(src);
   fclose(dest);
   free(buf);
   free(dest_fname);

   return;
}

/*****************************************************************************
 * parse the MOI file
 * reference:
 * http://forum.camcorderinfo.com/bbs/t135224.html  (further down is English translation)
 * http://en.wikipedia.org/wiki/MOI_(file_format)
 ****************************************************************************/
int get_moi_info(moi_info_type *info, char *moi_fname) {
   FILE *infile;
   char str[64] = "";
   struct tm *tm_buf;
   struct stat mtime_buf;


   /* open input file */
   infile = fopen(moi_fname, "rb");
   if (infile == NULL) {
      fprintf(stderr, "%s: WARNING: cannot open .MOI file %s\n", this, moi_fname);
      fprintf(stderr, "   skipping...\n");
      return 0;
   }

   /* get file access time on the file */
   fstat(fileno(infile), &mtime_buf); 
   info->mtime = mtime_buf.st_mtime;
   tm_buf = localtime(&mtime_buf.st_mtime);
   info->mtime_year  = tm_buf->tm_year + 1900;
   info->mtime_mon   = tm_buf->tm_mon + 1;
   info->mtime_day   = tm_buf->tm_mday;
   info->mtime_hour  = tm_buf->tm_hour;
   info->mtime_min   = tm_buf->tm_min;
   info->mtime_sec   = tm_buf->tm_sec;
   sprintf(str, "%04d%02d%02d-%02d%02d%02d", 
         info->mtime_year, info->mtime_mon, info->mtime_day, info->mtime_hour, info->mtime_min, info->mtime_sec);
   strcpy(info->mtime_date_str, str);

   /* get info out of the MOI fie */

   /* Year offset: 0x0007 (byte 8) - not sure what's in 0x0006
    * values: D0 - FF = 2000 - 2074 */
   fseek(infile, 0x07, SEEK_SET);
   info->moi_year = ((unsigned char) fgetc(infile)) - 0xD0 + 2000;

   /* Month * offset: 0x0008 
    * values: 01 - 0C = Month 1 - 12 */
   fseek(infile, 0x08, SEEK_SET);
   info->moi_mon = (unsigned char) fgetc(infile);

   /* Day offset: 0x0009
    * values: 01 - 1F = Day 1 - 31 */
   fseek(infile, 0x09, SEEK_SET);
   info->moi_day = (unsigned char) fgetc(infile);

   /* Hour offset: 0x000A
    * values: 00 - 17 = Hour 0 - 23 */
   fseek(infile, 0x0A, SEEK_SET);
   info->moi_hour = (unsigned char) fgetc(infile);
   //hour--;  /* hour seems to be +1 from the file time */

   /* Minute offset: 0x000B
    * values: 00 - 3B = Minute 0 - 59 */
   fseek(infile, 0x0B, SEEK_SET);
   info->moi_min = (unsigned char) fgetc(infile);

   /* Aspect Ratio offset: 0x0080
    * NOTE:
    * For my Panasonic SDR-H18, the values are 40 = 4:3, 44 = 16:9
    * wikipeida reference above (and others) say: values: 51 = 4:3, 55 = 16:9 */
   fseek(infile, 0x80, SEEK_SET);
   info->aspect_ratio = (unsigned char) fgetc(infile);
   if ( info->aspect_ratio == 0x40 || info->aspect_ratio == 0x50 || info->aspect_ratio == 0x51 )
      strcpy(info->aspect_ratio_str, "4:3");
   else if ( info->aspect_ratio == 0x44 || info->aspect_ratio == 0x54 || info->aspect_ratio == 0x55 )
      strcpy(info->aspect_ratio_str, "16:9");
   else {
      fprintf(stderr, "ERROR: Unknown aspect ratio value in MOI file: %02X\n", info->aspect_ratio);
      return 0;
   }


   fclose(infile);

   //fprintf(stdout, "%s date: %02d/%02d/%d %02d:%02d\n", fname, month, day, year, hour, minute);
   sprintf(str, "%04d%02d%02d-%02d%02d\0", 
         info->moi_year, info->moi_mon, info->moi_day, info->moi_hour, info->moi_min);
   strcpy(info->moi_date_str,str);

   if ( (verbose >= 2) || info_only ) {
      printf("%s: MOI Info  (%s):\n", this, moi_fname);
      printf("   moi_date_str   = %s\n", info->moi_date_str);
      printf("   mtime_date_str = %s\n", info->mtime_date_str);
      printf("   aspect_ratio   = 0x%X (%s)\n", info->aspect_ratio, info->aspect_ratio_str);
   }

   return 1;
}


/*****************************************************************************
 * Create the mpeg file from the mod/moi files.  Read MOD file, scanning for
 * sequence headers and setting aspect ratio in each before writing out as an
 * mpeg.
 *
 * The aspect ratio is a field in each Sequence Header in the mpeg file. The 
 * Sequence Header is defined by the signature 0x00 0x00 0x01 0xB3
 *
 * The aspect ratio is stored as a code in offset 7 of the header.
 * Aspect ratio is in the upper nibble and frame rate in the lower.
 *
 * Sequence header format (the part we care about):
 * |   byte 4      |     byte 5    |    byte 6     |      byte 7           |...
 * |7 6 5 4 3 2 1 0 7 6 5 4|3 2 1 0 7 6 5 4 3 2 1 0| 7  6  5  4 |3  2  1  0|...
 * |horizontal size        |vertical size          |aspect ratio|frame rate|...
 *
 * Code  Aspect Ratio   Frame Rate           (also sort of defines TV System)
 * 0     forbidden      forbidden
 * 1     1:1            24000/1001 (23.976)
 * 2     4:3            24
 * 3     16:9           25                   (PAL)
 * 4     2.21:1         30000/1001 (29.97)   (NTSC)
 * 5     reserved       30
 * 6     reserved       50
 * 7     reserved       60000/1001 (59.94)
 * 8     reserved       60
 * 9     reserved       reserved
 * :
 * 15    reserved       reserved
 *
 * reference: 
 * http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html#seq
 *
 ****************************************************************************/
void make_mpeg(char *mod_fname, char *output_dir, moi_info_type *info) {
   FILE *mpeg, *mod;
   char *mpeg_fname;
   unsigned char reference_seqh[] = { 0,0,0,0,0,0,0,0,0,0,0,0 };
   unsigned char sig[] = { 0x00, 0x00, 0x01, 0xB3 };  /* mpeg sequence header signature */
   unsigned char *buf, *p, *stop, *end, *hold;        /* buffer pointers */
   int br=0, bw=0, blksize=RW_BLOCK_SIZE, chunksize=0;/* bytes read, bytes written, block size, tail end of block */
   int blk=0, seqh=0, i;                              /* block count, sequence header count, general counter */
   unsigned char arfr, ar, fr;                        /* seqh offset 7, aspect ratio, frame rate */
   long long int tbw=0;                               /* total bytes written */



   buf = (char *) malloc(RW_BLOCK_SIZE);
   mpeg_fname = (char *) malloc(MAX_PATH_LEN);
   if (buf == NULL || mpeg_fname == NULL ) {
      fprintf(stderr, "%s: unable to allocate memory for buffers in make_mpeg\n", this);
      exit(1);
   }

   /* build the mpeg file name */
   if ( date_to_use == MTIME_DATE )
      sprintf(mpeg_fname, "%s/mov-%s.mpg", output_dir, info->mtime_date_str);
   else 
      sprintf(mpeg_fname, "%s/mov-%s.mpg", output_dir, info->moi_date_str);

   if ( verbose >= 2 )
      printf("%s: creating mpeg file %s\n", this, mpeg_fname);

   /* test to see if the file already exists */
   if ( noclobber ) {
      if (mpeg = fopen(mpeg_fname, "r")) {
         fclose(mpeg);
         if ( verbose >= 1 )
            fprintf(stderr, "   skipping... file exists and noclobber is on\n");
         free(buf);
         free(mpeg_fname);
         return;
      }
   }

   /* open mpeg file */
   if ( (mpeg = fopen(mpeg_fname, "wb")) == NULL ) {
      fprintf(stderr, "%s: unable to open %s\n", this, mpeg_fname);
      perror(mpeg_fname);
      exit(1);
   }
   /* open mod file */
   if ( (mod = fopen(mod_fname, "rb")) == NULL ) {
      fprintf(stderr, "%s: unable to open %s\n", this, mod_fname);
      perror(mod_fname);
      exit(1);
   }

   /* copy data from mod file to the mpeg file */
   /*
   while( (br = fread(buf, 1, RW_BLOCK_SIZE, mod)) > 0 ) {
      bw = fwrite(buf, 1, br, mpeg);
      if ( bw < 0 || ferror(mpeg) ) {
         perror("write failed");
         exit(1);
      }
   }
   */

   if ( verbose >= 3 ) 
      printf("%s: processing MOD file in %d byte blocks\n", this, blksize );

   hold = buf;
   while ( (br = fread(buf+chunksize, 1, blksize, mod)) > 0 ) {
     blk++; 
      /* 
       * We want to stop scanning within a headers+signature from the end of
       * the block.  Then move this remaining chunk to the beginning of the
       * buffer, set buffer pointer at the end of this chunk and do our next
       * fread.  Not sure what to do about the last block - but I doubt there
       * is a sequence header at the very end of the file...  At least I hope
       * not.
       */
      p = end = buf;
      end = buf + br + chunksize;  /* this had better add up! */
      stop = end - 8;  /* signature + 4 bytes of header */

      if ( verbose >= 4 )
         printf("%s: blk(%d) br=%d, end-buf=%ld, stop-buf=%ld, end-stop=%ld, buf-hold=%ld\n",
               this, blk, br, end-buf, stop-buf, end-stop, buf-hold);
      
      /* scan through the buffer */
      while ( p <= stop ) {

         /* skip ahead until we find the first byte of our signature. */
         while ( *p != sig[0] && p < stop )
            p++;

         /* found signature */
         if ( p[0] == sig[0] && p[1] == sig[1] && p[2] == sig[2] && p[3] == sig[3] ) {
            seqh++;

            if ( verbose >= 5 ) {
               /* print out the entire sequence header */
               printf("%s: (blk:%d seqh:%d)", this, blk, seqh);
               for (i=0; i<12; i++) printf(" %02X", *(p + i));
               printf("\n");
            }
            
            /* use first sequence header we come to as our reference header
             *
             * If all other sequence headers do not match our reference
             * sequence header, we skip them.  I do this because I've found
             * some sequence header signatures followed by non-standard data in
             * some MOD files.  I assume that it is a random occurrence of the
             * signature in some other data section.  Needs more research, but
             * this seems to works for now.
             */
            if ( reference_seqh[3] == 0 ) {
               memcpy(reference_seqh, p, 12);

               if ( verbose >= 3 ) {
                  printf("%s: found first sequence header, using as reference [", this);
                  for (i=0; i<12; i++) printf(" %02X", *(p + i));
                  printf(" ]\n");
               }

               /* 
                * define arfr - the aspect-ratio|frame-rate byte (offset 7) that we will
                * use in every other sequence header we find.
                *
                * grab seq header offset 7. This has aspect ratio in the upper nibble,
                * and frame rate in the lower nibble. We keep whatever the frame rate
                * is and set the aspect ratio to whatever our MOI file said it should be
                */
               fr = *(p + 7);  
               fr |= 0xF0;  /* keep frame rate - set upper nibble to 1111 */
               if ( strcmp(info->aspect_ratio_str, "1:1") == 0 )
                  arfr = 0x1F & fr;
               else if ( strcmp(info->aspect_ratio_str, "4:3") == 0 )
                  arfr = 0x2F & fr;
               else if ( strcmp(info->aspect_ratio_str, "16:9") == 0 )
                  arfr = 0x3F & fr;
               else if ( strcmp(info->aspect_ratio_str, "2.21:1") == 0 )
                  arfr = 0x4F & fr;
               else {
                  fprintf(stderr, "ERROR: invalid aspect ratio in MOI info structure (%s)\n", info->aspect_ratio_str);
                  exit(1);
               }
              
               if ( verbose >= 3 ) {
                  ar = fr = *(p + 7);  
                  ar >>= 4;    /* shift upper bits to the lower nibble */
                  ar &= 0x0F;  /* blank out the upper nibble */
                  fr &= 0x0F;  /* ditto */
                  printf("%s: found MOD seqh offset 7 = 0x%02X, aspect ratio = 0x%02X (%s), frame rate = 0x%02X (%s)\n",
                        this, *(p + 7), ar, mpeg_seqh_ar_codes[ar], fr, mpeg_seqh_fr_codes[fr]);
                  printf("%s: using MPEG seqh offset 7 = 0x%02X\n", this, arfr);
               }
            } /* ref seqh */

            /* We've got a sequence header signature, check the rest of the
             * header against our reference header. If it does not match, skip.
             * See note above about this. */
            if ( memcmp(reference_seqh, p, 12) != 0 ) {
               if ( verbose >= 3 ) {
                  printf("%s: found sequence header signature followed by non standard data\n   [", this);
                  for (i=0; i<12; i++) printf(" %02X", *(p + i));
                  printf(" ] skipping...\n");
               }
               p++;
               continue;
            }

            /* set aspect ratio/frame rate */
            if ( verbose >= 4 )
               printf("%s: setting aspect ratio (sequence header %d)\n", this, seqh);
            p += 7;
            *p = arfr;

         }  /* end found seqh */
         p++;

      }  /* end scan through block */

      /* account for last increment block scan loop */
      p--;  

      /* write out the buffer */
      bw = fwrite(buf, 1, p - buf, mpeg);
      tbw += bw;
      if ( bw < 0 || ferror(mpeg) ) {
         perror("write failed");
         exit(1);
      }
      if ( verbose >= 4 )
         printf("%s: blk(%d) bw=%ld, tbw=%lld \n", this, blk, p - buf, tbw);

      /* since seqh may span blocks, move last bit of data from end of buffer
       * to the beginning of next buffer */
      chunksize = end - p;
      blksize = RW_BLOCK_SIZE - chunksize;

      if ( verbose >= 5 )
         printf("%s: blk(%d) Bytes to end of block: %d, RW_BLOCK_SIZE: %d, next blocksize: %d\n", 
               this, blk, chunksize, RW_BLOCK_SIZE, blksize);

      memmove(buf, p, chunksize);
   }  /* end fread */


   /* write out the last bit of buffer */
   bw = fwrite(buf, 1, chunksize, mpeg);
   tbw += bw;
   if ( bw < 0 || ferror(mpeg) ) {
      perror("write failed");
      exit(1);
   }
   if ( verbose >= 4 )
      printf("%s: blk(%d) bw=%ld, tbw=%lld \n", this, blk, p - buf, tbw);


   /* wrap up */
   if ( (fclose(mpeg)) < 0 ) {
      fprintf(stderr, "%s: unable to close %s\n", this, mpeg_fname);
      perror(mpeg_fname);
      exit(1);
   }
   if ( (fclose(mod)) < 0 ) {
      fprintf(stderr, "%s: unable to close %s\n", this, mod_fname);
      perror(mod_fname);
      exit(1);
   }

   free(buf);
   free(mpeg_fname);
}


/*****************************************************************************
 * Return true if fname is one of the file types we are looking for
 * suffixes is an array of file suffixes to look for.
 * last entry must be null
 * case of suffix strings is unimportant (.txt or .TXT will both match)
 * for example, call with
 *   char mytypes[] = { ".txt", ".dat", NULL };
 *   is_file_type(fname, mytypes);
 ****************************************************************************/
int is_file_type(char *fname, char **suffixes) {
   char **p;
   int nlen, slen;

   nlen = strlen(fname);
   for (p = suffixes; *p; p++) {
      slen = strlen(*p);
      if ( nlen >= slen && strncasecmp(fname + nlen - slen, *p, slen) == 0 )
         return 1;
   }
   return 0;
}





/*****************************************************************************
 * remove newline and/or carriage return from end of a string 
 * return length of new modified string 
 ****************************************************************************/
static int chomp(char *s) {
   char *p;

   p = s + strlen(s) - 1; 

   while (p >= s && (*p == '\n' || *p == '\r'))
      *(p--) = 0;

   return (p - s + 1);
}

/*****************************************************************************
 * Return true if file NAME is a directory.
 ****************************************************************************/
static int isdir(char *fname) {
   struct stat st;
 
   if (stat(fname, &st)) {
      perror(fname);
      return 0;
   }
   return(S_ISDIR(st.st_mode));
}

/*****************************************************************************
 * Given the MOD fname, if an MOI file with the same dir/name exists, set
 * moi_fname and return true, else false.
 ****************************************************************************/
int locate_moi(char *moi_fname, char *mod_fname) {
   FILE *moi;
   size_t len;

   /* srip .MOD, and replace with .MOI */
   len = strlen(mod_fname) - 4;
   memcpy(moi_fname, mod_fname, len);
   memcpy(moi_fname + len, ".MOI\0", 5);   

   //printf("   Looking for %s...", moi_fname);
   if (moi = fopen(moi_fname, "rb")) {
      fclose(moi);
      return 1;
   }

   moi_fname[0] = '\0';
   return 0;
}



/*****************************************************************************
 * Return true if NAME should not be recursed into
 ****************************************************************************/
int ignore_ent(char *fname) {
   static char *ignored_entries[] = { ".", "..", NULL };
   char **p;

   for (p = ignored_entries; *p; p++) {
      if (strcmp (fname, *p) == 0)
         return 1;
   }
   return 0;
}




/*****************************************************************************
 * print usage message
 ****************************************************************************/
void usage() {
   //fprintf(stderr, "usage: %s [-h] from_dir to_dir\n", this);
      
   printf(" NAME\n");
   printf("    %s - creates an mpeg file from an MOD/MOI file pair, or reports\n", this);
   printf("    on information stored in the MOI file.\n");
   printf("\n");
   printf(" SYNOPSIS\n");
   printf("    Information only:\n");
   printf("    %s -i -f /path/to/source/dir/file.MOI\n", this);
   printf("    %s -i -s /path/to/source/dir\n", this);
   printf("\n");
   printf("    Convert to mpeg:\n");
   printf("    %s -f /path/to/source/dir/file.MOD -d /path/to/destination/dir\n", this);
   printf("    %s -s /path/to/source/dir -d /path/to/destination/dir\n", this);
   printf("\n");
   printf(" DESCRITION\n");
   printf("    This program combines .MOD/.MOI file pairs from the src_dir into an mpeg\n");
   printf("    in the dest_dir, correctly setting the aspect ratio in the .mpg file\n");
   printf("    headers, and using the date as a file name.\n");
   printf("\n");
   printf("    Certain brands of digital, tapeless camcorders, (JVC, Panasonic and\n");
   printf("    Cannon) save two files for each video.  The video file, with an extension\n");
   printf("    of .MOD, is an MPEG-2 and the metadata file, with an extension of .MOI.\n");
   printf("    Software that ships with these camcorders, combines the two files when\n");
   printf("    extracting from the camcorder, however as far as I am aware, they are not\n");
   printf("    available on Linux.\n");
   printf("\n");
   printf("    You can, if you like, just copy the .MOD files from your camcorder to\n");
   printf("    your computer and rename the .MOD to .mpg (or .mpeg).  This will work as\n");
   printf("    long as you have recorded in 4:3 aspect ratio.  However, if you have\n");
   printf("    recorded in 16:9 ratio, the aspect ratio will not be set correctly in the\n");
   printf("    .MOD file.\n");
   printf("\n");
   printf("    I have only tested this with my camcorder (Panasonic SDR-H18) on Ubuntu 10.04,\n");
   printf("    11.04 and 12.04\n");
   printf("\n");
   printf(" REFERENCE\n");
   printf("    File formats: \n");
   printf("    MOD   : http://en.wikipedia.org/wiki/MOD_and_TOD\n");
   printf("    MOI   : http://en.wikipedia.org/wiki/MOI_(file_format)\n");
   printf("    MPEG  : http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html\n");
   printf("    MPEG-2: http://en.wikipedia.org/wiki/MPEG-2\n");
   printf("\n");
   printf(" OPTIONS\n");
   printf("    -f, --mod-file=path/to/file_name.MOD\n");
   printf("             Convert only the single MOD file. File name can be either relative\n");
   printf("             or absolute path.\n");
   printf("\n");
   printf("    -s, --src-dir=path/to/dir\n");
   printf("             Source directory. Will convert all MOD/MOI pairs found in this dir.\n");
   printf("             Path can be either relative or absolute.\n");
   printf("             NOTE - this is currently recursive!\n");
   printf("\n");
   printf("    -d, --des-dir=path/to/dir\n");
   printf("             Destination directory. All files will be saved in this directory.\n");
   printf("             Path can be either relative or absolute.\n");
   printf("\n");
   printf("    -r, --recursive\n");
   printf("             When used with -d, will process all subdirectories as well\n");
   printf("\n");
   printf("    -i, --info\n");
   printf("             Prints information from MOI file only, does not convert MOD file\n");
   printf("\n");
   printf("    -c, --clobber\n");
   printf("             Allow overwriting of mpeg files with the same name. Default\n");
   printf("             behavior is to skip (not convert) if an mpeg file with the same\n");
   printf("             name already exists.\n");
   printf("\n");
   printf("    -m, --no-make-dirs\n");
   printf("             By default a directory structure will be created in the destination\n");
   printf("             dir based on file time, in the format dest_dir/YYYY/MM/DD. If you\n");
   printf("             do not want this behavior, use this option, in which case all MPEGs\n");
   printf("             will be output in the dest_dir directory.\n");
   printf("\n");
   printf("    -t, --modification-time, --mtime\n");
   printf("             Use the modification time of the MOD file.  Default is to use\n");
   printf("             the time defined in the MOI file.\n");
   printf("\n");
   printf("    -v, --verbose\n");
   printf("             Print status messages and warnings to stdout. Repeat -v option\n");
   printf("             for more verbose output.  Anything above -vvv is mostly debug print.\n");
   printf("\n");
   printf("    -h, --help\n");
   printf("             Prints this help message\n");
   printf("\n");
   printf(" AUTHOR\n");
   printf("    Matt Jeffery - mjeffe@gmail.com\n");
   printf("    Please report bugs or enhancement requests to the author.\n");
   printf("\n");
   printf("COPYRIGHT AND LICENSE\n");
   printf("\n");
   printf("    Copyright (c) 2011, Matt Jeffery (mjeffe@gmail.com)\n");
   printf("\n");
   printf("    This file is free software. You can redistribute it and/or modify it under the\n");
   printf("    terms of the FreeBSD License which is included in the header of the source\n");
   printf("\n");
   printf("\n");


#ifdef IGNORE_THIS_FOR_REFERENCE_ONLY
   static struct option long_options[] = {
      {"verbose",          no_argument,       0, 'v'},
      {"help",             no_argument,       0, 'h'},
      {"info",             no_argument,       0, 'i'},
      {"clobber",          no_argument,       0, 'c'},
      {"modification-time",no_argument,       0, 't'},
      {"mtime",            no_argument,       0, 't'},  /* alias */
      {"make-dirs",        no_argument,       0, 'm'},  /* c for create dirs */
      {"recursive",        no_argument,       0, 'r'},
      {"mod-file",         required_argument, 0, 'f'},
      {"src-dir",          required_argument, 0, 's'},
      {"dest-dir",         required_argument, 0, 'd'},
      {0, 0, 0, 0}
   };
#endif
}


/*
 * mkpath functions by Jonathan Leffler
 * from: http://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux
 */
static int do_mkdir(const char *path, mode_t mode)
{
    struct stat     st;
    int             status = 0;

    if (stat(path, &st) != 0)
    {
        /* Directory does not exist. EEXIST for race condition */
        if (mkdir(path, mode) != 0 && errno != EEXIST)
            status = -1;
    }
    else if (!S_ISDIR(st.st_mode))
    {
        errno = ENOTDIR;
        status = -1;
    }

    return(status);
}

/**
** mkpath - ensure all directories in path exist
** Algorithm takes the pessimistic view and works top-down to ensure
** each directory in path exists, rather than optimistically creating
** the last element and working backwards.
*/
int mkpath(const char *path, mode_t mode)
{
    char           *pp;
    char           *sp;
    int             status;
    char           *copypath = strdup(path);

    status = 0;
    pp = copypath;
    while (status == 0 && (sp = strchr(pp, '/')) != 0)
    {
        if (sp != pp)
        {
            /* Neither root nor double slash in path */
            *sp = '\0';
            status = do_mkdir(copypath, mode);
            *sp = '/';
        }
        pp = sp + 1;
    }
    if (status == 0)
        status = do_mkdir(path, mode);
    free(copypath);
    return (status);
}


