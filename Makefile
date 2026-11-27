CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -O2 -std=c11

PREFIX  := $(HOME)/.local
BINDIR  := $(PREFIX)/bin
EXT_UUID := tromonibar@tromonibar-linux
EXTDIR  := $(HOME)/.local/share/gnome-shell/extensions/$(EXT_UUID)
AUTOSTART_DIR := $(HOME)/.config/autostart

.PHONY: all test install uninstall clean

all: build/tromonibar

build/tromonibar: src/main.c src/layout.c src/layout.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ src/main.c src/layout.c

build/test_layout: tests/test_layout.c src/layout.c src/layout.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ tests/test_layout.c src/layout.c

test: build/test_layout build/tromonibar
	./build/test_layout
	sh tests/test_hdmi.sh

install: all
	install -d $(BINDIR) $(EXTDIR) $(AUTOSTART_DIR)
	install -m 0755 build/tromonibar $(BINDIR)/tromonibar
	install -m 0644 extension/metadata.json $(EXTDIR)/metadata.json
	install -m 0644 extension/extension.js $(EXTDIR)/extension.js
	sed 's|^Exec=tromonibar |Exec=$(BINDIR)/tromonibar |' autostart/tromonibar-apply.desktop > $(AUTOSTART_DIR)/tromonibar-apply.desktop
	chmod 0644 $(AUTOSTART_DIR)/tromonibar-apply.desktop
	@echo "Installed binary to $(BINDIR)/tromonibar and extension to $(EXTDIR)"

uninstall:
	rm -f $(BINDIR)/tromonibar
	rm -rf $(EXTDIR)
	rm -f $(AUTOSTART_DIR)/tromonibar-apply.desktop

clean:
	rm -rf build
