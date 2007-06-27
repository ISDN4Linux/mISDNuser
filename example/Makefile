LIBINCL	:= $(INCLUDEDIR)/mISDNlib.h
PROGS	:= testcon loghex
# PROGS   := testcon testnet loadfirm logger

all: $(PROGS)

install:
	for i in $(PROGS) ; do \
		install -m 755 $$i $(INSTALL_PREFIX)/usr/bin ;\
	done

testcon: testcon.o

testcon.o : testcon.c ../include/l3dss1.h

loghex: loghex.o
