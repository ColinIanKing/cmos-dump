cmos-dump: cmos-dump.o

CFLAGS += -O2 -Wall -Wextra

BINDIR=/usr/sbin

clean:
	rm -f cmos-dump cmos-dump.o

install:
	mkdir -p ${DESTDIR}${BINDIR}
	cp cmos-dump ${DESTDIR}${BINDIR}

snap: clean
	rm -f *.snap.*
	snapcraft clean
	snapcraft
