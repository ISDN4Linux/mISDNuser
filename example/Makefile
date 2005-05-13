LIBINCL	:= $(INCLUDEDIR)/mISDNlib.h $(INCLUDEDIR)/mISDNif.h
PROGS	:= testcon loadfirm
# PROGS   := testcon testnet testcon_l2 loadfirm logger

all: $(PROGS)

testcon: testcon.o $(mISDNLIB)

testnet: testnet.o $(mISDNLIB)

testcon_l2: testcon_l2.o $(mISDNLIB)

loadfirm: loadfirm.o $(mISDNLIB)

logger: logger.o $(mISDNLIB)


testcon.o : testcon.c ../include/l3dss1.h $(LIBINCL)

testnet.o : testnet.c ../include/l3dss1.h $(LIBINCL)

testcon_l2.o : testcon_l2.c ../include/l3dss1.h $(LIBINCL)

loadfirm.o: loadfirm.c $(LIBINCL)

logger.o: logger.c $(LIBINCL)


clean:
	rm -f *.o *~ DEADJOE

distclean: clean
	rm -f *.a $(PROGS)
