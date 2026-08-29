CC ?= cc
CFLAGS ?= -O2 -g
CPPFLAGS += -Isrc -D_GNU_SOURCE
CSTD ?= -std=c11
WARNFLAGS ?= -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
ifeq ($(WERROR),1)
WARNFLAGS += -Werror
endif
LDLIBS += -lmosquitto

PREFIX ?= /usr/local
SYSCONFDIR ?= /etc
SYSTEMDUNITDIR ?= /etc/systemd/system

SRC = src/gm10d.c src/config.c src/serial.c src/stats.c src/metrics.c src/mqtt.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean check install uninstall

all: gm10d

gm10d: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CSTD) $(WARNFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) gm10d tests/test_stats tests/test_serial_settle tests/test_metrics_http

check: tests/test_stats tests/test_serial_settle tests/test_metrics_http
	./tests/test_stats
	./tests/test_serial_settle
	./tests/test_metrics_http

tests/test_stats: tests/test_stats.c src/stats.c src/stats.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CSTD) $(WARNFLAGS) -o $@ tests/test_stats.c src/stats.c

tests/test_serial_settle: tests/test_serial_settle.c src/serial.c src/serial.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CSTD) $(WARNFLAGS) -o $@ tests/test_serial_settle.c src/serial.c

tests/test_metrics_http: tests/test_metrics_http.c src/metrics.c src/metrics.h src/stats.c src/stats.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CSTD) $(WARNFLAGS) -o $@ tests/test_metrics_http.c src/metrics.c src/stats.c

install: gm10d
	install -D -m 0755 gm10d $(DESTDIR)$(PREFIX)/sbin/gm10d
	install -D -m 0644 gm10d.conf.example $(DESTDIR)$(SYSCONFDIR)/gm10d.conf.example
	install -D -m 0644 gm10d.service $(DESTDIR)$(SYSTEMDUNITDIR)/gm10d.service

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/sbin/gm10d
	rm -f $(DESTDIR)$(SYSCONFDIR)/gm10d.conf.example
	rm -f $(DESTDIR)$(SYSTEMDUNITDIR)/gm10d.service
