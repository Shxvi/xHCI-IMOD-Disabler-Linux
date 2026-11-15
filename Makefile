CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread
SRC_DIR = src
BIN = xhci-imod-disabler

SRCS = $(SRC_DIR)/main.cpp
HEADERS = $(SRC_DIR)/xhci_common.hpp \
          $(SRC_DIR)/pci_models.hpp \
          $(SRC_DIR)/usb_device.hpp \
          $(SRC_DIR)/controller.hpp \
          $(SRC_DIR)/backend.hpp \
          $(SRC_DIR)/backup.hpp \
          $(SRC_DIR)/tui.hpp

PREFIX ?= /usr/local

all: $(BIN)

$(BIN): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) $(SRCS) -o $(BIN)

VERSION ?= 1.2
DIST_NAME = xHCI-IMOD-Disabler-Linux-v$(VERSION)

clean:
	rm -f $(BIN) $(DIST_NAME).tar.gz
	rm -rf $(DIST_NAME)
	$(MAKE) -C kernel-module clean 2>/dev/null || true

dist: $(BIN)
	rm -rf $(DIST_NAME) $(DIST_NAME).tar.gz
	mkdir -p $(DIST_NAME)/systemd $(DIST_NAME)/kernel-module
	cp $(BIN) install.sh Makefile README.md README.ru.md LICENSE $(DIST_NAME)/
	cp systemd/xhci-imod.service $(DIST_NAME)/systemd/
	cp kernel-module/* $(DIST_NAME)/kernel-module/
	tar -czvf $(DIST_NAME).tar.gz $(DIST_NAME)
	rm -rf $(DIST_NAME)
	@echo "Release archive created: $(DIST_NAME).tar.gz"

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)/etc/systemd/system
	install -m 644 systemd/xhci-imod.service $(DESTDIR)/etc/systemd/system/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
	rm -f $(DESTDIR)/etc/systemd/system/xhci-imod.service
	systemctl daemon-reload 2>/dev/null || true

kmod:
	$(MAKE) -C kernel-module

.PHONY: all clean install uninstall kmod dist
