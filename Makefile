# sff - simple file finder

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

EXTFNNAME = sff-extfunc
EXTFNPREFIX = ${PREFIX}/libexec/sff

# includes and libs
INCS = -I/usr/include/ncursesw
LIBS = -lncursesw

# flags
CPPFLAGS = -DDEBUG
CFLAGS   = -std=c11 -pedantic -Wall -Wextra -Wshadow -O2 ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}

# compiler and linker
CC = cc

# ===============================================================

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
	rm -f sff ${OBJ} sff.1.gz sff*.tar.gz

dist: clean
	mkdir -p sff
	cp -R CHANGELOG.md LICENSE Makefile README.md config.h plugins sff.1 ${SRC} ${EXTFNNAME} sff/
	tar -cf sff.tar sff
	gzip sff.tar
	rm -rf sff

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	install -m 755 sff ${DESTDIR}${PREFIX}/bin/
	mkdir -p ${DESTDIR}${EXTFNPREFIX}
	cp -fR ${EXTFNNAME} plugins ${DESTDIR}${EXTFNPREFIX}/
	chmod -R 755 ${DESTDIR}${EXTFNPREFIX}
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	gzip -fk sff.1
	install -m 644 sff.1.gz ${DESTDIR}${MANPREFIX}/man1/

uninstall:
	rm -rf ${DESTDIR}${PREFIX}/bin/sff \
		${DESTDIR}${EXTFNPREFIX} \
		${DESTDIR}${MANPREFIX}/man1/sff.1.gz

.PHONY: all options clean dist install uninstall
