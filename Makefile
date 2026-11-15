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

clean:
	rm -f $(BIN)
	$(MAKE) -C kernel-module clean 2>/dev/null || true

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

.PHONY: all clean install uninstall kmod
