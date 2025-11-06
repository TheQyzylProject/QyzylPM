# ──────────────────────────────
# QPM (Qyzyl Package Manager)
# ──────────────────────────────
# Version: 1.1.1 (Clean Animation)
# https://qyzyl.xyz
# ──────────────────────────────

PREFIX      ?= /usr
BINDIR      = $(PREFIX)/bin
SYSCONFDIR  = /etc
LOGDIR      = /var/log/qpm
CACHEDIR    = /var/cache/qpm

CC          = gcc
CFLAGS      = -Wall -O2
LDFLAGS     = -lcurl

SRC         = qpm.c
BIN         = qpm

# ──────────────────────────────
.PHONY: all clean install uninstall rebuild

all:
	@echo "Compiling QPM..."
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)
	@echo "Build complete → ./$(BIN)"

clean:
	@echo "Cleaning build artifacts..."
	rm -f $(BIN)
	@echo "Clean done."

install: all
	@echo "Installing QPM into $(DESTDIR)$(BINDIR)"
	install -Dm755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	@echo "Ensuring directories..."
	install -d $(DESTDIR)$(LOGDIR)
	install -d $(DESTDIR)$(CACHEDIR)
	@if [ ! -f "$(DESTDIR)$(SYSCONFDIR)/qpm.conf" ]; then \
		echo "Creating default config..."; \
		echo "mirror=https://mirrors.qyzyl.xyz/qpm/" > $(DESTDIR)$(SYSCONFDIR)/qpm.conf; \
	fi
	@echo "QPM installed successfully."

uninstall:
	@echo "Uninstalling QPM..."
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)
	rm -rf $(DESTDIR)$(LOGDIR)
	rm -rf $(DESTDIR)$(CACHEDIR)
	rm -f $(DESTDIR)$(SYSCONFDIR)/qpm.conf
	@echo "Uninstall complete."

rebuild: clean all
	@echo "Rebuilt QPM cleanly."

# ──────────────────────────────
# End of file
