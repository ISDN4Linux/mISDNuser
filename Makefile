#
# Set this to your local copy of mISDN
#
MISDNDIR := /usr/src/mqueue/mISDN

#
# Change this to create an install prefix for the shared libs, programms and
# includes
#
INSTALL_PREFIX := /
export INSTALL_PREFIX

MISDNINCLUDEDIR := $(MISDNDIR)/include
export MISDNINCLUDEDIR

mISDN_DIR := $(PWD)
export mISDN_DIR

INCLUDEDIR := $(mISDN_DIR)/include
export INCLUDEDIR

CFLAGS:= -g -Wall -O2 -I $(INCLUDEDIR) -I $(MISDNINCLUDEDIR)
CFLAGS+= -D CLOSE_REPORT=1
ifeq ($(shell uname -m),x86_64)
CFLAGS        += -fPIC
endif
export CFLAGS

mISDNLIB	:= $(PWD)/lib/libmISDN.a
mISDNNETLIB	:= $(PWD)/i4lnet/libmisdnnet.a
export mISDNLIB
export mISDNNETLIB

SUBDIRS := lib example

SUBDIRS += $(shell if test -d i4lnet ; then echo i4lnet; fi)
SUBDIRS += $(shell if test -d tenovis ; then echo tenovis; fi)
SUBDIRS += $(shell if test -d voip ; then echo voip; fi)

LIBS := lib/libmISDN.a

all: test_misdn_includes
	make TARGET=$@ subdirs


install_path:
	mkdir -p $(INSTALL_PREFIX)/usr/bin/
	mkdir -p $(INSTALL_PREFIX)/usr/include/mISDNuser/

install: install_path all
	make TARGET=install subdirs
	cp include/*.h $(INSTALL_PREFIX)/usr/include/mISDNuser/


subdirs:
	set -e; for i in $(SUBDIRS) ; do $(MAKE) -C $$i $(TARGET); done

clean:  
	make TARGET=$@ subdirs
	rm -f *.o *~ DEADJOE $(INCLUDEDIR)/*~ $(INCLUDEDIR)/DEADJOE

distclean: clean
	make TARGET=$@ subdirs
	rm -f *.o *~ testlog

MAINDIR := $(shell basename $(PWD))
ARCHIVDIR = /usr/src/packages/SOURCES
ARCHIVOPT := -v
# VERSION := $(shell date +"%Y%m%d")
VERSION := 20030423

ARCHIVNAME := $(ARCHIVDIR)/$(MAINDIR)-$(VERSION).tar.bz2

archiv: distclean
	cd ../; tar c $(ARCHIVOPT) -f - $(MAINDIR) | bzip2 > $(ARCHIVNAME)

basearchiv: ARCHIVOPT += --exclude i4lnet --exclude voip --exclude tenovis
basearchiv: ARCHIVNAME := $(ARCHIVDIR)/$(MAINDIR)_base-$(VERSION).tar.bz2
basearchiv: archiv

mainarchiv: ARCHIVOPT += --exclude voip --exclude tenovis
mainarchiv: ARCHIVNAME := $(ARCHIVDIR)/$(MAINDIR)_main-$(VERSION).tar.bz2
mainarchiv: archiv

tenovisarchiv: ARCHIVOPT += --exclude voip --exclude i4lnet
tenovisarchiv: ARCHIVNAME := $(ARCHIVDIR)/$(MAINDIR)_tenovis-$(VERSION).tar.bz2
tenovisarchiv: archiv

voiparchiv: ARCHIVOPT += --exclude tenovis
voiparchiv: ARCHIVNAME := $(ARCHIVDIR)/$(MAINDIR)_voip-$(VERSION).tar.bz2
voiparchiv: archiv


test_misdn_includes:
	@if ! echo "#include <linux/mISDNif.h>" | gcc -I$(MISDNINCLUDEDIR) -C -E - >/dev/null ; then echo -e "\n\nYou either don't seem to have installed mISDN properly\nor you haven't set the MISDNDIR variable in this very Makefile.\n\nPlease either install mISDN or set the MISDNDIR properly\n"; exit 1; fi


