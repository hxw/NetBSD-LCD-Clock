# Makefile

DESTIR ?= ""
PREFIX ?= /usr/pkg

PATH := ${PATH}:${PREFIX}/bin
.export PATH

FREETYPE2_CFLAGS != pkg-config --cflags freetype2
FREETYPE2_LDFLAGS != pkg-config --libs freetype2

# set up toolchain options
CFLAGS += ${FREETYPE2_CFLAGS}
CFLAGS += -Wall -Werror -std=c99 -Wextra
CFLAGS += -I.

LDFLAGS += ${FREETYPE2_LDFLAGS}

RM = rm -f

# paths to sources
SRCS = gpio.c spi.c ili9486.c unicode.c clock-main.c


# default target
.PHONY: all
all: lcd_clock

.PHONY: install
install: all
	install -o root -g wheel -m 0555 lcd_clock "${PREFIX}/libexec/"
	install -o root -g wheel -m 0555 rc.d/lcd_clock "/etc/rc.d/"


# low-level driver
DRIVER_OBJECTS = gpio.o spi.o ili9486.o unicode.o
TEST_OBJECTS = main.o ${DRIVER_OBJECTS}
CLOCK_OBJECTS = clock-main.o ${DRIVER_OBJECTS}

# build test program
CLEAN_FILES += lcd_clock
lcd_clock: depend ${CLOCK_OBJECTS}
	${CC} ${CFLAGS} ${LDFLAGS} -o "$@" ${CLOCK_OBJECTS}


# tests
.PHONY: test
test: unicode.c unicode.h
	${RM} test_unicode
	cc -DTESTING=1 -o test_unicode unicode.c
	./test_unicode
	${RM} test_unicode
CLEAN_FILES += test_unicode

# compute dependencies
.PHONY: depend
depend: .depend
.depend:
	mkdep -- ${CFLAGS} ${SRCS}

# clean up
.PHONY: clean
clean:
	${RM} ${CLEAN_FILES}
	${RM} *.o *~ .depend

# dependencies
.dinclude ".depend"
