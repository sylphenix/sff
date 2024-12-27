# sff - simple file finder
VERSION = 0.9

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

EXTFNNAME = sff-extfunc
EXTFNPREFIX = ${PREFIX}/libexec

# includes and libs
INCS =
LIBS = -lncursesw

# flags
CPPFLAGS = -DDEBUG -D_DEFAULT_SOURCE -D_BSD_SOURCE -DEXTFNNAME=\"${EXTFNNAME}\" -DEXTFNPREFIX=\"${EXTFNPREFIX}\" -DVERSION=\"${VERSION}\"
CFLAGS   = -std=c11 -pedantic -Wall -Wextra -Wshadow -Wno-deprecated-declarations -Os ${CPPFLAGS}
LDFLAGS  = ${LIBS}

# compiler and linker
CC = cc

#====================================

SRC = sff.c
OBJ = ${SRC:.c=.o}

all: options sff

options:
	@echo sff build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h

config.h:
	cp config.def.h $@

sff: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}
	rm sff.o

clean:
	rm -f sff ${OBJ} sff-${VERSION}.tar.gz

dist:
	mkdir -p sff-${VERSION}
	cp -R LICENSE Makefile README.md ${SRC} ${EXTFNNAME} config.h sff.1 sff-${VERSION}/
	tar -cf sff-${VERSION}.tar sff-${VERSION}
	gzip sff-${VERSION}.tar
	rm -rf sff-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f sff ${DESTDIR}${PREFIX}/bin/
	chmod 755 ${DESTDIR}${PREFIX}/bin/sff
	mkdir -p ${DESTDIR}${EXTFNPREFIX}
	cp -f ${EXTFNNAME} ${DESTDIR}${EXTFNPREFIX}/
	chmod 755 ${DESTDIR}${EXTFNPREFIX}/${EXTFNNAME}
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -f sff.1 ${DESTDIR}${MANPREFIX}/man1/sff.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/sff.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/sff\
		${DESTDIR}${EXTFNPREFIX}/${EXTFNNAME}\
		${DESTDIR}${MANPREFIX}/man1/sff.1

.PHONY: all options clean dist install uninstall
