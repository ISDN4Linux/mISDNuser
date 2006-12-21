CC = gcc
AR = ar
RANLIB = ranlib

all: libmISDN.a libmISDN_pic.a libmISDN.so

install:
	install -m 644 libmISDN.so $(INSTALL_PREFIX)/usr/lib
	install -m 644 libmISDN_pic.a $(INSTALL_PREFIX)/usr/lib
	install -m 644 libmISDN.a $(INSTALL_PREFIX)/usr/lib

LIBMISDN_OBJS = device.o layer.o stack.o status.o
LIBMISDN_PICOBJS = $(LIBMISDN_OBJS:%.o=%.lo)

ifeq ($(shell uname -m),x86_64)
CFLAGS         += -fPIC
endif

libmISDN_pic.a: $(LIBMISDN_PICOBJS)
	$(AR) cru $@ $^
	$(RANLIB) $@

libmISDN.a: $(LIBMISDN_OBJS)
	$(AR) cru $@ $^
	$(RANLIB) $@

libmISDN.so: $(LIBMISDN_OBJS)
	$(CC) $(CFLAGS) -shared -Xlinker -x -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

.c.lo:
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

device.o device.lo: device.c ../include/mISDNlib.h
layer.o layer.lo: layer.c ../include/mISDNlib.h
stack.o stack.lo: stack.c ../include/mISDNlib.h
status.o status.lo: status.c ../include/mISDNlib.h 

clean:
	rm -f libmISDN.a libMISDN_pic.a libmISDN.so
	rm -f *.o *.lo *~ DEADJOE

distclean: clean
	rm -f *.a
 
.SUFFIXES: .lo
