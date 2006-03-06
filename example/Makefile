LIBINCL	:= $(INCLUDEDIR)/mISDNlib.h
PROGS	:= testcon testlayer3 loadfirm sendhwctrl
# PROGS   := testcon testnet testcon_l2 loadfirm logger

all: $(PROGS)

install:
	for i in $(PROGS) ; do \
		install -m 755 $$i $(INSTALL_PREFIX)/usr/bin ;\
	done

testcon: testcon.o $(mISDNLIB)

testlayer3: testlayer3.o $(mISDNLIB)

testnet: testnet.o $(mISDNLIB)

testcon_l2: testcon_l2.o $(mISDNLIB)

loadfirm: loadfirm.o $(mISDNLIB)

sendhwctrl: sendhwctrl.o $(mISDNLIB)

logger: logger.o $(mISDNLIB)


testcon.o : testcon.c ../include/l3dss1.h $(LIBINCL)

testlayer3.o : testlayer3.c ../include/l3dss1.h $(LIBINCL)

testnet.o : testnet.c ../include/l3dss1.h $(LIBINCL)

testcon_l2.o : testcon_l2.c ../include/l3dss1.h $(LIBINCL)

loadfirm.o: loadfirm.c $(LIBINCL)

sendhwctrl.o: sendhwctrl.c $(LIBINCL)

logger.o: logger.c $(LIBINCL)


clean:
	rm -f *.o *~ DEADJOE

distclean: clean
	rm -f *.a $(PROGS)
