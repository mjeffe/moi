# ---------------------------------------------------------------------
# $Id: Makefile 825 2010-03-10 22:05:02Z mjeffe $
#
# Build and install
#
# Make sure $MAKE_MACHINE is set in your environment before running
#
# 	export MAKE_MACHINE=LINUX
# 	make
# 	make install
# ---------------------------------------------------------------------

C_UTILS = moi

# -----------------------------------------
# C comiler settings

# LINUX Settings
CC_LINUX = cc
LINUX_LARGE_FILE_SUPPORT = -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS_LINUX = -O2 -I. $(LINUX_LARGE_FILE_SUPPORT)
LDFLAGS_LINUX =
TARGETS_LINUX = $(C_UTILS)

# SOLARIS settings
CC_SOLARIS =
SOLARIS_LARGE_FILE_SUPPORT = 
CFLAGS_SOLARIS =
LDFLAGS_SOLARIS =
TARGETS_SOLARIS = $(C_UTILS)


# Set up variables depending on machine type
CC      = $(CC_$(MAKE_MACHINE))
CFLAGS  = $(CFLAGS_$(MAKE_MACHINE))
LDFLAGS = $(LDFLAGS_$(MAKE_MACHINE))
TARGETS = $(TARGETS_$(MAKE_MACHINE))
# -----------------------------------------


all: $(TARGETS)

moi : moi.o
	$(CHECK)
	$(CC) $(CFLAGS) $(LDFLAGS) -o moi moi.c

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

