# sff - simple file finder

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

EXTFNNAME = sff-extfunc
EXTFNPREFIX = ${PREFIX}/lib/sff

# includes and libs
INCS =
LIBS = -lncursesw
STATIC_LIBS = -lncursesw -ltinfo

# flags
CPPFLAGS = -DDEBUG
CFLAGS   = -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -fstack-protector-strong ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS}
STATIC_LDFLAGS = -static ${STATIC_LIBS}

# compiler and linker
CC = cc

# ===========================================================================

SRC = sff.c
OBJ = ${SRC:.c=.o}

all: options sff

static: options sff-static

options:
	@echo sff build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

config.h:
	cp config.def.h $@

${OBJ}: config.h

%.o: %.c
	${CC} -c ${CFLAGS} -o $@ $<

sff: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

sff-static: ${OBJ}
	${CC} -o $@ ${OBJ} ${STATIC_LDFLAGS}

clean:
	rm -f sff sff-static ${OBJ} sff*.tar.gz

dist: clean
	mkdir -p sff
	cp -af CHANGELOG.md README.md LICENSE Makefile config.h plugins sff.1 ${SRC} ${EXTFNNAME} sff/
	tar -caf sff.tar.gz sff
	rm -rf sff

install: all
	mkdir -p "${DESTDIR}${PREFIX}/bin"
	install -m 755 sff "${DESTDIR}${PREFIX}/bin/"
	mkdir -p "${DESTDIR}${EXTFNPREFIX}"
	cp -Rf ${EXTFNNAME} plugins "${DESTDIR}${EXTFNPREFIX}/"
	chmod -R 755 "${DESTDIR}${EXTFNPREFIX}"
	mkdir -p "${DESTDIR}${MANPREFIX}/man1"
	install -m 644 sff.1 "${DESTDIR}${MANPREFIX}/man1/"
	gzip -nf "${DESTDIR}${MANPREFIX}/man1/sff.1"

uninstall:
	rm -rf "${DESTDIR}${PREFIX}/bin/sff" \
		"${DESTDIR}${EXTFNPREFIX}" \
		"${DESTDIR}${MANPREFIX}/man1/sff.1.gz"

.PHONY: all static options clean dist install uninstall
