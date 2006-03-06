
lib: libtenovis.a

libtenovis.a: tenovis_device.o tenovis_intern.o
	rm -f $@
	ar -r $@ $^
	ar -s $@

tenovis_device.o : tenovis_device.c tenovis.h tenovis_int.h \
			../../include/mISDNlib.h

tenovis_intern.o : tenovis_intern.c tenovis.h tenovis_int.h \
			../../include/mISDNlib.h

clean:
	rm -f *.o *~ DEADJOE
	rm -f libtenovis.a

distclean: clean
	rm -f *.a

