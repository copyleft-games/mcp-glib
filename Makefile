# mcp-glib Makefile
#
# Copyright (C) 2025 Copyleft Games
# SPDX-License-Identifier: AGPL-3.0-or-later

# Project metadata
PROJECT     := mcp-glib
VERSION     := 1.0.0
API_VERSION := 1.0
LICENSE     := AGPLv3

# Directories
SRCDIR      := src
INCDIR      := mcp
BUILDDIR    := build
TESTDIR     := tests
EXAMPLESDIR := examples

# Compiler configuration
CC          := gcc
CSTD        := -std=gnu89
WARNINGS    := -Wall -Wextra -Wno-unused-parameter
DEBUG       := -g -O0
CFLAGS      := $(CSTD) $(WARNINGS) $(DEBUG) -fPIC -I.

# Dependencies via pkg-config
PKG_DEPS    := glib-2.0 gobject-2.0 gio-2.0 gio-unix-2.0 json-glib-1.0 libsoup-3.0 libdex-1
CFLAGS      += $(shell pkg-config --cflags $(PKG_DEPS))
LDFLAGS     := $(shell pkg-config --libs $(PKG_DEPS))

# Library naming
LIB_NAME    := lib$(PROJECT)-$(API_VERSION)
LIB_SO      := $(LIB_NAME).so
LIB_A       := $(LIB_NAME).a

# GObject Introspection
GIR_SCANNER := g-ir-scanner
GIR_COMPILER:= g-ir-compiler
GIR_FILE    := Mcp-$(API_VERSION).gir
TYPELIB     := Mcp-$(API_VERSION).typelib

# Sources
SRCS        := $(wildcard $(SRCDIR)/*.c)
OBJS        := $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
DEPS        := $(OBJS:.o=.d)

# Headers
HDRS        := $(wildcard $(INCDIR)/*.h)

# Tests
TEST_SRCS   := $(wildcard $(TESTDIR)/*.c)
TEST_BINS   := $(TEST_SRCS:$(TESTDIR)/%.c=$(BUILDDIR)/%)

# Examples
EXAMPLE_SRCS := $(wildcard $(EXAMPLESDIR)/*.c)
EXAMPLE_BINS := $(EXAMPLE_SRCS:$(EXAMPLESDIR)/%.c=$(BUILDDIR)/%)

# Targets
.PHONY: all clean test examples gir install uninstall docs info

all: $(BUILDDIR)/$(LIB_SO) $(BUILDDIR)/$(LIB_A) $(BUILDDIR)/$(PROJECT)-$(API_VERSION).pc

# Shared library
$(BUILDDIR)/$(LIB_SO): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

# Static library
$(BUILDDIR)/$(LIB_A): $(OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

# Object files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEPS)

# Tests
test: $(TEST_BINS)
	@echo "Running tests..."
	@for t in $(TEST_BINS); do \
		echo "  Running $$t"; \
		$$t || exit 1; \
	done
	@echo "All tests passed."

$(BUILDDIR)/test-%: $(TESTDIR)/test-%.c $(BUILDDIR)/$(LIB_SO)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS) \
		-Wl,-rpath,$(CURDIR)/$(BUILDDIR)

# Examples
examples: $(EXAMPLE_BINS)

$(BUILDDIR)/%: $(EXAMPLESDIR)/%.c $(BUILDDIR)/$(LIB_SO)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS) -lm \
		-Wl,-rpath,$(CURDIR)/$(BUILDDIR)

# pkg-config file
$(BUILDDIR)/$(PROJECT)-$(API_VERSION).pc: $(PROJECT)-$(API_VERSION).pc.in
	@mkdir -p $(dir $@)
	sed -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@VERSION@|$(VERSION)|g' \
	    -e 's|@API_VERSION@|$(API_VERSION)|g' \
	    $< > $@

# GObject Introspection
gir: $(BUILDDIR)/$(GIR_FILE) $(BUILDDIR)/$(TYPELIB)

$(BUILDDIR)/$(GIR_FILE): $(BUILDDIR)/$(LIB_SO) $(HDRS) $(SRCS)
	@mkdir -p $(dir $@)
	$(GIR_SCANNER) \
		--namespace=Mcp \
		--nsversion=$(API_VERSION) \
		--identifier-prefix=Mcp \
		--symbol-prefix=mcp \
		--library=$(PROJECT)-$(API_VERSION) \
		--library-path=$(BUILDDIR) \
		--include=GObject-2.0 \
		--include=Gio-2.0 \
		--include=Json-1.0 \
		--include=Soup-3.0 \
		--include=Dex-1 \
		--pkg=glib-2.0 \
		--pkg=gobject-2.0 \
		--pkg=gio-2.0 \
		--pkg=json-glib-1.0 \
		--pkg=libsoup-3.0 \
		--pkg=libdex-1 \
		--output=$@ \
		--warn-all \
		--cflags-begin -DMCP_COMPILATION --cflags-end \
		-I. \
		$(HDRS) $(SRCS)

$(BUILDDIR)/$(TYPELIB): $(BUILDDIR)/$(GIR_FILE)
	$(GIR_COMPILER) $< -o $@

# Installation
PREFIX      ?= /usr/local
LIBDIR      := $(PREFIX)/lib
INCLUDEDIR  := $(PREFIX)/include/$(PROJECT)-$(API_VERSION)
PKGCONFIGDIR:= $(LIBDIR)/pkgconfig
GIRDIR      := $(PREFIX)/share/gir-1.0
TYPELIBDIR  := $(LIBDIR)/girepository-1.0

install: all gir
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -d $(DESTDIR)$(PKGCONFIGDIR)
	install -d $(DESTDIR)$(GIRDIR)
	install -d $(DESTDIR)$(TYPELIBDIR)
	install -m 755 $(BUILDDIR)/$(LIB_SO) $(DESTDIR)$(LIBDIR)/
	install -m 644 $(BUILDDIR)/$(LIB_A) $(DESTDIR)$(LIBDIR)/
	install -m 644 $(HDRS) $(DESTDIR)$(INCLUDEDIR)/
	install -m 644 $(BUILDDIR)/$(PROJECT)-$(API_VERSION).pc $(DESTDIR)$(PKGCONFIGDIR)/
	install -m 644 $(BUILDDIR)/$(GIR_FILE) $(DESTDIR)$(GIRDIR)/
	install -m 644 $(BUILDDIR)/$(TYPELIB) $(DESTDIR)$(TYPELIBDIR)/
	ldconfig $(DESTDIR)$(LIBDIR) || true

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_SO)
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_A)
	rm -rf $(DESTDIR)$(INCLUDEDIR)
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/$(PROJECT)-$(API_VERSION).pc
	rm -f $(DESTDIR)$(GIRDIR)/$(GIR_FILE)
	rm -f $(DESTDIR)$(TYPELIBDIR)/$(TYPELIB)

# Documentation (placeholder)
docs:
	@echo "Documentation generation not yet implemented"

# Clean
clean:
	rm -rf $(BUILDDIR)

# Debug info
info:
	@echo "PROJECT:     $(PROJECT)"
	@echo "VERSION:     $(VERSION)"
	@echo "SRCS:        $(SRCS)"
	@echo "OBJS:        $(OBJS)"
	@echo "HDRS:        $(HDRS)"
	@echo "TEST_BINS:   $(TEST_BINS)"
	@echo "CFLAGS:      $(CFLAGS)"
	@echo "LDFLAGS:     $(LDFLAGS)"
