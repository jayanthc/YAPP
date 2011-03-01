#
# Makefile for Yet Another Pulsar Processor
#
# Created by Jayanth Chennamangalam on 2008.11.17
#

# C compiler and flags
CC = gcc
CFLAGS = -Wall -pedantic
CFLAGS_C_DEBUG = -g -c $(CFLAGS)
CFLAGS_C_RELEASE = -O3 -c $(CFLAGS)
#CFLAGS_C = $(CFLAGS_C_DEBUG)
CFLAGS_C = $(CFLAGS_C_RELEASE)
CFLAGS_L_DEBUG = -g $(CFLAGS)
CFLAGS_L_RELEASE = -O3 $(CFLAGS)
#CFLAGS_L = $(CFLAGS_L_DEBUG)
CFLAGS_L = $(CFLAGS_L_RELEASE)

# Fortran compiler and flags
FC = gfortran       # change to 'g77' to use the Fortran 77 compiler 
FFLAGS = -c

# enable/disable the debug flag
#DDEBUG = -DDEBUG

# linker flags
LFLAGS_PGPLOT_DIR = -L/usr/lib
LFLAGS_PGPLOT = $(LFLAGS_PGPLOT_DIR) -lpgplot -lcpgplot
LFLAGS_MATH = -lm
LFLAGS_SNDFILE = -lsndfile

# directories
SRCDIR = .
IDIR = .
BINDIR = /usr/local/bin

# command definitions
DELCMD = rm

all: yapp_makever \
	 yapp_version.o \
	 yapp_common.o \
	 set_colours.o \
	 yapp_viewdata.o \
	 yapp_viewdata \
	 yapp_viewmetadata.o \
	 yapp_viewmetadata \
	 tags

yapp_makever: $(SRCDIR)/yapp_makever.c
	$(CC) $(CFLAGS_L) $(SRCDIR)/yapp_makever.c -o $(IDIR)/$@
	$(IDIR)/yapp_makever
	$(DELCMD) $(IDIR)/yapp_makever

yapp_version.o: $(SRCDIR)/yapp_version.c
	$(CC) -c $(CFLAGS_C) $(SRCDIR)/yapp_version.c -o $(IDIR)/$@

ifeq ($(FC), g77)
DFC = -D_FC_F77_
set_colours.o: $(SRCDIR)/set_colours.f
	$(FC) $(FFLAGS) $(SRCDIR)/set_colours.f -o $(IDIR)/$@
else
DFC = -D_FC_F95_
set_colours.o: $(SRCDIR)/set_colours_f95.f
	$(FC) $(FFLAGS) $(SRCDIR)/set_colours_f95.f -o $(IDIR)/$@
endif

yapp_common.o: $(SRCDIR)/yapp_common.c
	$(CC) $(CFLAGS_C) $(DDEBUG) $(SRCDIR)/yapp_common.c -o $(IDIR)/$@

yapp_viewdata.o: $(SRCDIR)/yapp_viewdata.c
	$(CC) $(CFLAGS_C) $(DDEBUG) $(DFC) $(SRCDIR)/yapp_viewdata.c -o $(IDIR)/$@

yapp_viewdata: $(IDIR)/yapp_viewdata.o
	$(FC) $(IDIR)/yapp_viewdata.o $(IDIR)/yapp_version.o $(IDIR)/yapp_common.o \
        $(IDIR)/set_colours.o $(LFLAGS_PGPLOT) $(LFLAGS_MATH) -o $(BINDIR)/$@

yapp_viewmetadata.o: $(SRCDIR)/yapp_viewmetadata.c
	$(CC) $(CFLAGS_C) $(SRCDIR)/yapp_viewmetadata.c -o $(IDIR)/$@

yapp_viewmetadata: $(IDIR)/yapp_viewmetadata.o
	$(CC) $(SRCDIR)/yapp_viewmetadata.o $(IDIR)/yapp_version.o $(IDIR)/yapp_common.o \
		$(LFLAGS_MATH) -o $(BINDIR)/$@

# create the tags file
tags: $(SRCDIR)/yapp*.*
	ctags $(SRCDIR)/yapp*.*

install:
	@echo Nothing to install.

clean:
	$(DELCMD) $(IDIR)/yapp_version.c
	$(DELCMD) $(IDIR)/yapp_version.o
	$(DELCMD) $(IDIR)/yapp_common.o
	$(DELCMD) $(IDIR)/set_colours.o
	$(DELCMD) $(IDIR)/yapp_viewdata.o
	$(DELCMD) $(IDIR)/yapp_viewmetadata.o

