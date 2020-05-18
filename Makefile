LDFLAGS =
CFLAGS  =

AR = ar
CP = cp
RM = rm

libfdb.a: fdb.o
	$(AR) cr $@ $^

install:
	$(CP) libfdb.a /usr/lib/
	$(CP) fdb.h /usr/include/

clean:
	$(RM) fdb.o libfdb.a

.PHONY: install clean
