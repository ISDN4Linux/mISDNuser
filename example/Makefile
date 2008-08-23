LIBINCL	:= $(INCLUDEDIR)/mISDNlib.h
PROGS	:= testcon loghex testlayer3 cleanl2 misdnportinfo testlayer1
# PROGS   := testcon testnet loadfirm logger

all: $(PROGS)

mISDNLIB := ../layer3/libmisdn.so
LIBPTHREAD := -lpthread
LIBMISDN := -lmisdn

testlayer3 :	LDFLAGS += -L $(LIBDIR) $(LIBPTHREAD) $(LIBMISDN)

CC := gcc -g $(CFLAGS)

install:
	for i in $(PROGS) ; do \
		install -m 755 $$i $(INSTALL_PREFIX)/usr/bin ;\
	done

misdnportinfo: misdnportinfo.o

testcon: testcon.o

testlayer3: testlayer3.o

testcon.o : testcon.c ../include/q931.h ../include/mISDNif.h ../include/compat_af_isdn.h

testlayer3.o: testlayer3.c ../include/q931.h ../include/mlayer3.h ../include/mISDNif.h ../include/compat_af_isdn.h

testlayer1: testlayer1.o
testlayer1.o: testlayer1.c ../include/mISDNif.h  ../include/compat_af_isdn.h

loghex: loghex.o

loghex.o: loghex.c ../include/mISDNif.h  ../include/compat_af_isdn.h

cleanl2: cleanl2.o

cleanl2.o: cleanl2.c ../include/mISDNif.h ../include/compat_af_isdn.h

clean:
	rm -f *.o *.lo *~ $(PROGS) DEADJOE

distclean: clean
	rm -f $(PROGS)
	

