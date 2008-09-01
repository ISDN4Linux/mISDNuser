CC = gcc
AR = ar
RANLIB = ranlib

all: libmisdn.a libmisdn_pic.a libmisdn.so

install:
	install -d $(INSTALL_PREFIX)$(INSTALL_LIBDIR)
	install -m 644 libmisdn.a $(INSTALL_PREFIX)$(INSTALL_LIBDIR)
	install -m 644 libmisdn_pic.a $(INSTALL_PREFIX)$(INSTALL_LIBDIR)
	install -m 644 libmisdn.so $(INSTALL_PREFIX)$(INSTALL_LIBDIR)

MISDN_OBJ = debug.o mbuffer.o q931.o fsm.o mtimer.o mlayer3.o layer3.o dss1user.o dss1net.o


MISDN_PICOBJ = $(ISDNNET_OBJ:%.o=%.lo)

ifeq ($(shell uname -m),x86_64)
CFLAGS         += -fPIC
endif

CFLAGS += $(EXTRA_CFLAGS) 

libmisdn_pic.a: $(MISDN_PICOBJ)
	$(AR) cru $@ $^
	$(RANLIB) $@

libmisdn.a: $(MISDN_OBJ)
	$(AR) cru $@ $^
	$(RANLIB) $@

libmisdn.so: $(MISDN_OBJ)
	$(CC) $(CFLAGS) -shared -Xlinker -x -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

.c.lo:
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

$(INCLUDEDIR)/mlayer3.h: $(INCLUDEDIR)/mISDNif.h
debug.o debug.lo:	debug.c debug.h
mbuffer.o mbuffer.lo:	mbuffer.c $(INCLUDEDIR)/mbuffer.h $(INCLUDEDIR)/mlayer3.h debug.h
q931.o q931.lo:		q931.c $(INCLUDEDIR)/mbuffer.h $(INCLUDEDIR)/mlayer3.h $(INCLUDEDIR)/q931.h debug.h
fsm.o fsm.lo:		fsm.c fsm.h mtimer.h debug.h
mtimer.o mtimer.lo:	mtimer.c mtimer.h mlist.h debug.h
mlayer3.o mlayer3.lo:	mlayer3.c $(INCLUDEDIR)/mbuffer.h $(INCLUDEDIR)/mlayer3.h $(INCLUDEDIR)/q931.h $(INCLUDEDIR)/mISDNif.h  $(INCLUDEDIR)/compat_af_isdn.h mlist.h debug.h layer3.h
dss1user.o dss1user.lo:	dss1user.c $(INCLUDEDIR)/mbuffer.h $(INCLUDEDIR)/mlayer3.h $(INCLUDEDIR)/q931.h dss1.h layer3.h debug.h
dss1net.o dss1net.lo:	dss1net.c $(INCLUDEDIR)/mbuffer.h $(INCLUDEDIR)/mlayer3.h $(INCLUDEDIR)/q931.h dss1.h layer3.h debug.h
layer3.o layer3.lo:	layer3.c $(INCLUDEDIR)/mbuffer.h $(INCLUDEDIR)/mlayer3.h $(INCLUDEDIR)/q931.h $(INCLUDEDIR)/mISDNif.h mlist.h mtimer.h layer3.h debug.h


clean:
	rm -f *.o *.lo *~ DEADJOE 
	rm -f libmisdn.a libmisdn_pic.a libmisdn.so

distclean: clean
	rm -f *.a

.SUFFIXES: .lo
