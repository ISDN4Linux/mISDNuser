LIBINCL	:= $(INCLUDEDIR)/mISDNlib.h

TENOVISLIB := lib/libtenovis.a

TENOVISINC := lib/tenovis.h

SUBDIRS := lib

PROGS	:= testlib tstlib

all: sublib $(PROGS)

install:
	for i in $(PROGS) ; do \
		install -m 755 $$i $(INSTALL_PREFIX)/usr/bin ;\
	done

testlib: testlib.o $(TENOVISLIB) $(mISDNLIB)

tstlib: tstlib.o $(TENOVISLIB) $(mISDNLIB)


testlib.o : testlib.c ../include/l3dss1.h $(LIBINCL) $(TENOVISINC)

tstlib.o : tstlib.c $(LIBINCL) $(TENOVISINC)

sublib:
	$(MAKE) -C lib lib

subdirs:
	set -e; for i in $(SUBDIRS) ; do $(MAKE) -C $$i $(TARGET); done

clean:
	make TARGET=$@ subdirs
	rm -f *.o *~ DEADJOE
	rm -f testlib tstlib

distclean: clean
	make TARGET=$@ subdirs
	rm -f *.a $(PROGS)
