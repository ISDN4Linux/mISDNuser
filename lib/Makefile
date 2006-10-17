SOMAJOR=0
SOMINOR=0.0
SOVERSION=$(SOMAJOR).$(SOMINOR)

all: libmISDN.a libmISDN.so.$(SOVERSION)

install:
	install -m 755 libmISDN.so.$(SOVERSION) $(INSTALL_PREFIX)/$(LIBDIR)
	ln -sf libmISDN.so.$(SOVERSION) $(INSTALL_PREFIX)/$(LIBDIR)/libmISDN.so.$(SOMAJOR)
	ln -sf libmISDN.so.$(SOVERSION) $(INSTALL_PREFIX)/$(LIBDIR)/libmISDN.so
	install -m 644 libmISDN.a $(INSTALL_PREFIX)/$(LIBDIR)

LIBMISDN_OBJS = device.o layer.o stack.o status.o

libmISDN.so.$(SOVERSION): $(LIBMISDN_OBJS)
	$(CC) -shared -Xlinker -x -Wl,-soname,libmISDN.so.$(SOMAJOR) -o $@ $^

libmISDN.a: $(LIBMISDN_OBJS)
	ar cr $@ $^
	ar s $@

device.o : device.c ../include/mISDNlib.h
layer.o : layer.c ../include/mISDNlib.h
stack.o : stack.c ../include/mISDNlib.h
status.o : status.c ../include/mISDNlib.h 

clean:
	rm -f *.o *~ DEADJOE
	rm -f libmISDN.a

distclean: clean
	rm -f *.a
 
