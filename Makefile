mISDN_DIR := $(PWD)
export mISDN_DIR

INCLUDEDIR := $(mISDN_DIR)/include
export INCLUDEDIR

CFLAGS:= -g -Wall -O2 -I $(INCLUDEDIR)
CFLAGS+= -D CLOSE_REPORT=1
export CFLAGS

mISDNLIB	:= $(PWD)/lib/libmISDN.a
mISDNNETLIB	:= $(PWD)/i4lnet/libmisdnnet.a
export mISDNLIB
export mISDNNETLIB

SUBDIRS := lib example

# SUBDIRS += $(shell if test -d i4lnet ; then echo i4lnet; fi)
# SUBDIRS += $(shell if test -d tenovis ; then echo tenovis; fi)
# SUBDIRS += $(shell if test -d voip ; then echo voip; fi)

LIBS := lib/libmISDN.a

all:
	make TARGET=$@ subdirs

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

