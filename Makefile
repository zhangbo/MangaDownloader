CC?=gcc
PREFIX?=/usr/local
CRAWLER=CRAWLER
INSTALL_DIR=$(PREFIX)/bin
BUILD_DIST=build
OSNAME=$(shell uname -s | sed -e 's/[-_].*//g' | tr A-Z a-z)

S_SRC= EntryPoint.c crawler.c Downloader.c
ifeq ("$(OSNAME)", "darwin")
	LDFLAGS+=$(shell pkg-config --cflags --libs libxml-2.0 libcurl)
else
endif
S_OBJS=	$(S_SRC:.c=.o)

CFLAGS+=-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=-Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=-Wsign-compare -Iincludes
CFLAGS+=-DPREFIX='"$(PREFIX)"'


all: $(S_OBJS)
	mkdir -p $(BUILD_DIST)
	$(CC) $(S_OBJS) $(LDFLAGS) -o ./build/$(CRAWLER)

install:
	mkdir -p $(INSTALL_DIR)
	install -m 755 $(CRAWLER) $(INSTALL_DIR)/$(CRAWLER)

uninstall:
	rm -f $(INSTALL_DIR)/$(CRAWLER)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	find . -type f -name \*.o -exec rm {} \;
	rm -f $(BUILD_DIST)/$(CRAWLER)

.PHONY: clean