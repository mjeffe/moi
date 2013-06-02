/*****************************************************************************
 *
 * $Id$
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
 *    - Report on when the date contained in the .MOI file is very different
 *    from the .MOD file creation date.
 *    - Make block size a parameter.
 *    - Add ability to create date / time level directories
 *    - Add check to make sure mpeg file size is same as MOD file size
 *    - Add ability to use MOI date, but make unique file name if duplicate - may not mess with this
 *    - Add ability to extract aspect ratio, frame rate, etc, from mpeg or MOD
 *    - write log
 *    - add usage message
 *
 *
 * Reference:
 *    Reference to the MOI file format was found here:
 *    http://en.wikipedia.org/wiki/MOI_(file_format)
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
#include <sys/types.h> /* stat */
#include <sys/stat.h>  /* stat */
#include <unistd.h>    /* stat, NULL, etc */
#include <time.h>      /* localtime */
#include <dirent.h>    /* opendir, chdir, getcwd, etc */
#include <libgen.h>    /* basename */
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
int make_dirs = 0;           /* if set, create seperate directories for each date */
int date_to_use = MOI_DATE;  /* default to using date in MOI file */
int info_only = 0;           /* if set, don't copy, only report MOI info */
int noclobber = 1;           /* if set, do not overwrite existing mpeg files */
static char *ignored_entries[] = { ".", "..", NULL };
static char *searched_entries[] = { ".MOD", ".mod", ".Mod", NULL };
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
   "25",
   "30000/1001 (29.97)",
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
void get_moi_info(moi_info_type *info, char *fname);
int locate_moi(char *moi_fname, char *mod_fname);
static int chomp(char *s);
void make_dir(char *dir);
static int isdir(char *name);
int ignore_ent(char *name);
int is_search_ent(char *fname);
void process_dir(char *dirname);
void process_file(char *dir, char *fname);
void process_mod(char *dir, char *fname);
void make_mpeg(char *mod_fname, moi_info_type *info);
int set_mpeg_ar(FILE *mpeg, char *moi_ar_str);



/*****************************************************************************
 * MAIN
 ****************************************************************************/
int main(int argc, char *argv[]) {
   int c;
   char *src_dir = NULL;
   char *src_file = NULL, *src_file_base = NULL, *src_file_cpy1 = NULL, *src_file_cpy2 = NULL;
   /* getopt_long structures */
   int option_index = 0;
   static struct option long_options[] = {
      {"verbose",          no_argument,       0, 'v'},
      {"help",             no_argument,       0, 'h'},
      {"info",             no_argument,       0, 'i'},
      {"overwrite",        no_argument,       0, 'o'},
      {"modification-time",no_argument,       0, 'm'},
      {"mtime",            no_argument,       0, 'm'},  /* alias */
      {"separate-dirs",    no_argument,       0, 'c'},  /* c for create dirs */
      {"mod-file",         required_argument, 0, 'f'},
      {"src-dir",          required_argument, 0, 's'},
      {"dest-dir",         required_argument, 0, 'd'},
      {0, 0, 0, 0}
   };


   this = argv[0];


   optarg = NULL;
   //while ((c = getopt_long(argc, argv, "itvho", long_options, &option_index)) != -1 ) {
   while (1) {
      c = getopt_long(argc, argv, "itvhomf:s:d:", long_options, &option_index);

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
         case 'o':
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
         /* create directory for each date */
         case 'm':
            make_dirs = 1;
            fprintf(stderr, "--make-dirs is currently unimplemented\n");
            exit(1);
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
         case 'v':
            verbose++;
            break;
         case 'h':
            usage();
         default:
            fprintf(stderr,"%s: Unknown or invalid parameters\n", this);
            usage();
      }
   }


   /*
    * Validate arguments and options
    */

   if ( (argc > optind) ) {
      fprintf(stderr, "Error: unknown options or extra stuff on the command line.\n");
      usage();
   }

   /* strip trailing / if any from dest_dir */
   if ( dest_dir && dest_dir[strlen(dest_dir)-1] == '/' )
      dest_dir[strlen(dest_dir)-1] = '\0';

   /*
   if ( (argc == optind + 1) ) {
      src_dir = argv[optind];
   }
   else if ( argc != optind + 2 ) {
      src_dir = argv[optind];
      dest_dir = argv[optind+1];
   }
   */


   /* request for info, we only need input source */
   if ( info_only  && !(src_dir || src_file) ) {
      fprintf(stderr, "%s: Error: missing either -s or -f\n", this);
      exit(1);
   }

   /* full processing requested, we need src and des sources */
   if ( !dest_dir && !(src_dir || src_file) ) {
         fprintf(stderr, "%s: Error: missing -d, and either -s or -f\n", this);
         usage();
   } 

   /*
    * Do the work
    */
   if ( src_file ) {
      src_file_cpy1 = strdup(src_file);
      src_file_cpy2 = strdup(src_file);
      src_dir = dirname(src_file_cpy2);
      src_file_base = basename(src_file_cpy1);
      process_file(src_dir, src_file_base);
   }
   else
      process_dir(src_dir);

   return(0);
}


/*****************************************************************************
 * call process_file() on every file in dirname, recursively descend into
 * directories except . and ..
 ****************************************************************************/
void process_dir(char *dirname) {
   DIR *dir;
   struct dirent *ent;
   char cwd[2048];
   char newd[2048];

   if ( !getcwd(cwd, sizeof cwd) ) {
      perror("cannot save cwd\n");
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
   if ( verbose )
      printf("Processing directory:%s\n", newd);

   if ( !(dir = opendir(".")) ) {
      perror(dirname);
      exit(1);
   }
 
   while ( (ent = readdir(dir)) ) {
      if ( ignore_ent(ent->d_name) )
         continue;
      else if ( isdir(ent->d_name) ) {
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
   if ( is_search_ent(fname) )
      process_mod(dir, fname); 
}


/*****************************************************************************
 * 1) check for sister MOI file
 * 2) extract necessary info from MOI file
 * 3) create mpeg file in target dir
 ****************************************************************************/
void process_mod(char *dir, char *fname) {
   char *mod_fname, *moi_fname;
   moi_info_type *info;


   info = (moi_info_type *) malloc(sizeof(moi_info_type));
   mod_fname = (char *) malloc(MAX_PATH_LEN);
   moi_fname = (char *) malloc(MAX_PATH_LEN);
   if ( info == NULL || mod_fname == NULL || moi_fname == NULL ) {
      fprintf(stderr, "%s: unable to allocate memory for buffers in process_mod\n", this);
      exit(1);
   }

   sprintf(mod_fname, "%s/%s", dir, fname);
   if ( locate_moi(moi_fname, mod_fname) ) {
      get_moi_info(info, moi_fname);
      if ( !info_only )
         make_mpeg(mod_fname, info);
   }
   else
      fprintf(stderr, "Warning! Found %s with no matching .MOI file\n", mod_fname);

   free(info);
   free(mod_fname);
   free(moi_fname);
}


/*****************************************************************************
 * parse the MOI file
 * reference:
 * http://forum.camcorderinfo.com/bbs/t135224.html  (further down is English translation)
 * http://en.wikipedia.org/wiki/MOI_(file_format)
 ****************************************************************************/
void get_moi_info(moi_info_type *info, char *fname) {
   FILE *infile;
   char str[64] = "";
   struct tm *tm_buf;
   struct stat mtime_buf;


   
   /* open input file */
   infile = fopen(fname, "rb");
   if (infile == NULL) {
      fprintf(stderr, "%s: unable to open %s\n", this, fname);
      exit(1);
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
      fprintf(stderr, "WARNING: Unknown aspect ratio value in MOI file: %02X\n", info->aspect_ratio);
      exit(1);
   }


   fclose(infile);

   //fprintf(stdout, "%s date: %02d/%02d/%d %02d:%02d\n", fname, month, day, year, hour, minute);
   sprintf(str, "%04d%02d%02d-%02d%02d\0", 
         info->moi_year, info->moi_mon, info->moi_day, info->moi_hour, info->moi_min);
   strcpy(info->moi_date_str,str);

   if ( verbose || info_only ) {
      printf("MOI Info in (%s):\n", fname);
      printf("   moi_date_str   = %s\n", info->moi_date_str);
      printf("   mtime_date_str = %s\n", info->mtime_date_str);
      printf("   aspect_ratio   = 0x%X (%s)\n", info->aspect_ratio, info->aspect_ratio_str);
   }

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
 * Code  Aspect Ratio   Frame Rate
 * 0     forbidden      forbidden
 * 1     1:1            24000/1001 (23.976)
 * 2     4:3            24
 * 3     16:9           25
 * 4     2.21:1         30000/1001 (29.97)
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
void make_mpeg(char *mod_fname, moi_info_type *info) {
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
      sprintf(mpeg_fname, "%s/%s.mpg", dest_dir, info->mtime_date_str);
   else 
      sprintf(mpeg_fname, "%s/%s.mpg", dest_dir, info->moi_date_str);

   if ( verbose )
      fprintf(stdout, "Creating %s\n", mpeg_fname);

   /* test to see if the file already exists */
   if ( noclobber ) {
      if (mpeg = fopen(mpeg_fname, "r")) {
         fclose(mpeg);
         if ( verbose )
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
         printf("blk(%d) br=%d, end-buf=%d, stop-buf=%d, end-stop=%d, buf-hold=%d\n",
               blk, br, end-buf, stop-buf, end-stop, buf-hold);
      
      /* scan through the buffer */
      while ( p <= stop ) {

         /* skip ahead until we find the first byte of our signature. */
         while ( *p != sig[0] && p < stop )
            p++;

         /* found signature */
         if ( p[0] == sig[0] && p[1] == sig[1] && p[2] == sig[2] && p[3] == sig[3] ) {
            seqh++;

            if ( verbose >= 5 ) {
               /* debug - print out the entire sequence header */
               printf("   (blk:%d seqh:%d)", blk, seqh);
               for (i=0; i<12; i++) printf(" %02X", *(p + i));
               printf("\n");
            }
            
            /* First sequence header we come to is our reference header. */
            if ( reference_seqh[3] == 0 ) {
               memcpy(reference_seqh, p, 12);

               if ( verbose >= 2 )
                  printf("Setting aspect ratio\n");
               if ( verbose >= 3 ) {
                  printf("   reference sequence header [");
                  for (i=0; i<12; i++) printf(" %02X", *(p + i));
                  printf(" ]\n");
               }

               /* Seq header offset 7.  
                * Aspect ratio is in upper nibble | frame rate is in lower nibble */
               fr = *(p + 7);  
               fr |= 0xF0;  /* set upper nibble to 1111 */
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
              
               if ( verbose >= 2 ) {
                  ar = fr = *(p + 7);  
                  ar >>= 4;    /* shift upper bits to the lower nibble */
                  ar &= 0x0F;  /* blank out the upper nibble */
                  fr &= 0x0F;  /* ditto */
                  printf("   MOD: seqh offset 7 = 0x%02X, aspect ratio = 0x%02X (%s), frame rate = 0x%02X (%s)\n", 
                        *(p + 7), ar, mpeg_seqh_ar_codes[ar], fr, mpeg_seqh_fr_codes[fr]);
                  printf("   Setting mpeg seqh offset 7 = 0x%02X\n", arfr);
               }
            } /* ref seqh */

            /*
             * If all other sequence headers do not match our reference
             * sequence header, we skip them.  I do this because I've found
             * some sequence header signatures followed by non-standard data in
             * some MOD files.  I assume that it is a random occurrence of the
             * signature in some other data section.  Needs more research, but
             * this works for now.
             */
            if ( memcmp(reference_seqh, p, 12) != 0 ) {
               if ( verbose >= 3 ) {
                  printf("   found sequence header signature followed by non standard data\n   [");
                  for (i=0; i<12; i++) printf(" %02X", *(p + i));
                  printf(" ] skipping...\n");
               }
               p++;
               continue;
            }

            /* set aspect ratio/frame rate */
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
         printf("blk(%d) bw=%d, tbw=%lld \n", blk, p - buf, tbw);

      /* since seqh may span blocks, move last bit of data from end of buffer
       * to the beginning of next buffer */
      chunksize = end - p;
      blksize = RW_BLOCK_SIZE - chunksize;

      if ( verbose >= 4 )
         printf("blk(%d) Bytes to end of block: %d, RW_BLOCK_SIZE: %d, next blocksize: %d\n", 
               blk, chunksize, RW_BLOCK_SIZE, blksize);

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
      printf("blk(%d) bw=%d, tbw=%lld \n", blk, p - buf, tbw);


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
 ****************************************************************************/
int is_search_ent(char *fname) {
   char **p;
   int nlen, slen;

   nlen = strlen(fname);
   for (p = searched_entries; *p; p++) {
      slen = strlen(*p);
      if ( nlen >= slen && strncasecmp(fname + nlen - slen, *p, slen) == 0 )
         return 1;
   }
   return 0;
}

/*****************************************************************************
 * Check to make sure dest_dir exists.  If not, create it.
 ****************************************************************************/
void make_dir(char *dir) {
   mkdir(dir,0777);
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
 * Given the MOD fname, return true if an MOI file with same dir/name exists.
 ****************************************************************************/
int locate_moi(char *moi_fname, char *mod_fname) {
   FILE *moi;
   size_t len;

   /* srip .MOD, and replace with .MOI */
   len = strlen(mod_fname) - 4;
   memcpy(moi_fname, mod_fname, len);
   memcpy(moi_fname + len, ".MOI\0", 5);   

   //printf("   Looking for %s...", moi_fname);
   if (moi = fopen(moi_fname, "r")) {
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
#ifdef IGNORE_THIS
      {"verbose",          no_argument,       0, 'v'},
      {"help",             no_argument,       0, 'h'},
      {"info",             no_argument,       0, 'i'},
      {"overwrite",        no_argument,       0, 'o'},
      {"modification-time",no_argument,       0, 'm'},
      {"mtime",            no_argument,       0, 'm'},  /* alias */
      {"separate-dirs",    no_argument,       0, 'c'},  /* c for create dirs */
      {"mod-file",         required_argument, 0, 'f'},
      {"src-dir",          required_argument, 0, 's'},
      {"dest-dir",         required_argument, 0, 'd'},
      ;
#endif

   fprintf(stderr, "usage: %s [-h] from_dir to_dir\n", this);

      
 /*
 * NAME
 *    %s - creates an mpeg file from an MOD/MOI file pair, or reports
 *    on information stored in the MOI file.
 *
 * SYNOPSIS
 *    Information only:
 *    %s -i /path/to/source/dir/file.MOI
 *    %s -i /path/to/source/dir
 *
 *    Convert to mpeg:
 *    %s -f /path/to/source/dir/file.MOD -d /path/to/destination/dir
 *    %s -s /path/to/source/dir -d /path/to/destination/dir
 *
 * DESCRITION
 *    This program combines .MOD/.MOI file pairs from the src_dir into an mpeg
 *    in the dest_dir, correctly setting the aspect ratio in the .mpg file
 *    headers, and using the date as a file name.
 *
 *    Certain brands of digital, tapeless camcorders, (JVC, Panasonic and
 *    Cannon) save two files for each video.  The video file, with an extension
 *    of .MOD, is an MPEG-2 and the metadata file, with an extension of .MOI.
 *    Software that ships with these camcorders, combines the two files when
 *    extracting from the camcorder, however as far as I am aware, they are not
 *    available on Linux.
 *
 *    You can, if you like, just copy the .MOD files from your camcorder to
 *    your computer and rename the .MOD to .mpg (or .mpeg).  This will work as
 *    long as you have recorded in 4:3 aspect ratio.  However, if you have
 *    recorded in 16:9 ratio, the aspect ratio will not be set correctly in the
 *    .MOD file.
 *
 *    I have only tested this with my camcorder (Panasonic SDR-H18) on Ubuntu 10.04
 *
 *
 * REFERENCE
 *    File formats: 
 *    MOD : http://en.wikipedia.org/wiki/MOD_and_TOD_(video_format)
 *    MOI : http://en.wikipedia.org/wiki/MOI_(file_format)
 *    MPEG: http://dvd.sourceforge.net/dvdinfo/pes-hdr.html
 *
 */




   exit(1);
}


