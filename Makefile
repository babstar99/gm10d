CC ?= cc
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -D_GNU_SOURCE
CPPFLAGS += -Isrc
LDLIBS += -lmosquitto

PREFIX ?= /usr/local
SYSCONFDIR ?= /etc
SYSTEMDUNITDIR ?= /etc/systemd/system

SRC = src/gm10d.c src/config.c src/serial.c src/stats.c src/metrics.c src/mqtt.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean check install

all: gm10d

gm10d: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

clean:
	rm -f $(OBJ) gm10d tests/test_stats

check: tests/test_stats
	./tests/test_stats

tests/test_stats: tests/test_stats.c src/stats.c src/stats.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_stats.c src/stats.c

install: gm10d
	install -D -m 0755 gm10d $(DESTDIR)$(PREFIX)/sbin/gm10d
	install -D -m 0644 gm10d.conf.example $(DESTDIR)$(SYSCONFDIR)/gm10d.conf.example
	install -D -m 0644 gm10d.service $(DESTDIR)$(SYSTEMDUNITDIR)/gm10d.service
