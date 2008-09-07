LIBINCL	:= $(INCLUDEDIR)/mISDNlib.h
PROGS	:= testcon testlayer3 misdnportinfo testlayer1

all: $(PROGS)

mISDNLIB := ../lib/libmisdn.so
LIBPTHREAD := -lpthread
LIBMISDN := -lmisdn

testlayer3 :	LDFLAGS += -L $(LIBDIR) $(LIBPTHREAD) $(LIBMISDN)

CFLAGS += $(EXTRA_CFLAGS)

CC := gcc -g $(CFLAGS)

ifdef INSTALL_EXAMPLES
	DO_INSTALL= for i in $(PROGS) ; do \
		install -m 755 $$i $(INSTALL_PREFIX)/usr/bin ;\
	done
else
	DO_INSTALL=true
endif

install:
	$(DO_INSTALL)

misdnportinfo: misdnportinfo.o

testcon: testcon.o

testlayer3: testlayer3.o

testcon.o : testcon.c ../include/q931.h ../include/mISDNif.h ../include/compat_af_isdn.h

testlayer3.o: testlayer3.c ../include/q931.h ../include/mlayer3.h ../include/mISDNif.h ../include/compat_af_isdn.h

testlayer1: testlayer1.o
testlayer1.o: testlayer1.c ../include/mISDNif.h  ../include/compat_af_isdn.h

clean:
	rm -f *.o *.lo *~ $(PROGS) DEADJOE

distclean: clean
	rm -f $(PROGS)
	

