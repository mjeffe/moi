# ---------------------------------------------------------------------
# $Id: Makefile 825 2010-03-10 22:05:02Z mjeffe $
#
# This helps to install these files in the right place
# ---------------------------------------------------------------------

TARGETS = moi
SUPPORT = /home/mjeffe/src/sawmill/jamestools/support

# -----------------------------------------
# C comiler settings

# LINUX Settings
CC_LINUX = cc
LINUX_LARGE_FILE_SUPPORT = -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS_LINUX = -O2 -I. -I$(SUPPORT) $(LINUX_LARGE_FILE_SUPPORT)
LDFLAGS_LINUX =
TARGETS_LINUX = $(C_UTILS)

# Set up variables depending on machine type
CC      = $(CC_$(MAKE_MACHINE))
CFLAGS  = $(CFLAGS_$(MAKE_MACHINE))
LDFLAGS = $(LDFLAGS_$(MAKE_MACHINE))
TARGETS = $(TARGETS_$(MAKE_MACHINE))
# -----------------------------------------


#all: $(TARGETS)


moi : moi.o
	$(CHECK)
	$(CC) $(CFLAGS) $(LDFLAGS) -o moi moi.c

die.o: $(SUPPORT)/die.c
	$(CHECK)
	$(CC) $(CFLAGS) $(LDFLAGS2) -c $(SUPPORT)/die.c

wd.o: wd.c $(SUPPORT)/die.h

wd: wd.o die.o
	$(CHECK)
	$(CC) $(CFLAGS) $(LDFLAGS) -o wd wd.o die.o



install : $(UTILS)
# make necesary directories if they do not already exist
	if test ! -d $(HOME)/bin; then \
	   mkdir $(HOME)/bin; \
	fi

	cp $(UTILS) $(HOME)/bin/

clean:
	rm -f *.o
	rm -fr $(TARGETS)



CHECK= \
  @if [ "$(MAKE_MACHINE)" = "" ]; \
  then \
     echo "MAKE_MACHINE not set: This must be set before running make."; \
     exit 1; \
  elif [ "$(MAKE_MACHINE)" = "AIX" -a "$(OBJECT_MODE)" = "" ]; \
  then \
     echo "Compiling on AIX and OBJECT_MODE is not set: This must be set before running make."; \
     exit 1; \
  fi

