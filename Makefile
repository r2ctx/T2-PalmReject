CC ?= cc
CPPFLAGS ?= -Iinclude
CFLAGS ?= -O2 -Wall -Wextra
LDFLAGS ?=

TARGET := palmreject
SOURCES := src/main.c src/virtual_touchpad.c
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(SOURCES) include/virtual_touchpad.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(TARGET)"
