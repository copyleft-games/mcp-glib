# mcp-glib Makefile
#
# Copyright (C) 2025 Copyleft Games
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Build libmcp-glib with cross-compilation support

#=============================================================================
# Cross-Compilation Configuration
#=============================================================================
# Usage:
#   make                      # Native Linux build (x86_64 or aarch64)
#   make WINDOWS=1            # Cross-compile for Windows x64
#   make LINUX_ARM64=1        # Cross-compile for Linux ARM64
#   make CROSS=<prefix>       # Explicit cross-compiler prefix
#=============================================================================

WINDOWS ?= 0
LINUX_ARM64 ?= 0
CROSS ?=

# ARM64 sysroot location (Fedora's sysroot-aarch64-fc41-glibc package)
ARM64_SYSROOT ?= /usr/aarch64-redhat-linux/sys-root/fc41

# Set CROSS based on convenience variables
ifeq ($(WINDOWS),1)
    CROSS := x86_64-w64-mingw32
endif

ifeq ($(LINUX_ARM64),1)
    CROSS := aarch64-linux-gnu
endif

#=============================================================================
# Architecture Detection
#=============================================================================

ifneq ($(CROSS),)
    ifeq ($(CROSS),x86_64-w64-mingw32)
        TARGET_ARCH := x86_64
    else ifeq ($(CROSS),aarch64-linux-gnu)
        TARGET_ARCH := aarch64
    else
        TARGET_ARCH := unknown
    endif
else
    TARGET_ARCH := $(shell uname -m)
endif

#=============================================================================
# Toolchain Configuration
#=============================================================================

ifneq ($(CROSS),)
    CC := $(CROSS)-gcc
    AR := $(CROSS)-ar
    RANLIB := $(CROSS)-ranlib

    ifeq ($(CROSS),x86_64-w64-mingw32)
        # Windows cross-compilation uses mingw pkg-config wrapper
        PKG_CONFIG := $(CROSS)-pkg-config
        TARGET_PLATFORM := windows
    else ifeq ($(CROSS),aarch64-linux-gnu)
        # ARM64 cross-compilation uses sysroot with native pkg-config
        PKG_CONFIG_SYSROOT_DIR := $(ARM64_SYSROOT)
        PKG_CONFIG_PATH := $(ARM64_SYSROOT)/usr/lib64/pkgconfig:$(ARM64_SYSROOT)/usr/share/pkgconfig
        PKG_CONFIG := PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config
        SYSROOT_FLAGS := --sysroot=$(ARM64_SYSROOT)
        TARGET_PLATFORM := linux
    else
        PKG_CONFIG := pkg-config
        TARGET_PLATFORM := linux
    endif
else
    CC ?= gcc
    AR ?= ar
    RANLIB ?= ranlib
    PKG_CONFIG ?= pkg-config
    TARGET_PLATFORM := linux
    SYSROOT_FLAGS :=
endif

#=============================================================================
# Project Metadata
#=============================================================================

PROJECT     := mcp-glib
VERSION     := 1.0.0
API_VERSION := 1.0
LICENSE     := AGPLv3

#=============================================================================
# Directories
#=============================================================================

SRCDIR      := src
BUILDDIR    := build
TESTDIR     := tests
EXAMPLESDIR := examples

#=============================================================================
# Platform-Specific Configuration
#=============================================================================

ifeq ($(TARGET_PLATFORM),windows)
    # Windows: DLL + import library
    # No gio-unix, no libdex, no libsoup (HTTP/WebSocket transports excluded)
    # Note: stdio transport excluded for cross-compilation (mingw lacks gwin32inputstream.h)
    # When building natively on Windows with full SDK, stdio transport can be re-enabled
    PKG_DEPS    := glib-2.0 gobject-2.0 gio-2.0 json-glib-1.0
    LIB_STATIC  := lib$(PROJECT)-$(API_VERSION).a
    LIB_SHARED  := $(PROJECT)-$(API_VERSION).dll
    LIB_IMPORT  := lib$(PROJECT)-$(API_VERSION).dll.a
    EXE_EXT     := .exe
    SHARED_LDFLAGS = -shared -Wl,--out-implib,$(BUILDDIR)/$(LIB_IMPORT)
    # Exclude transports not available via mingw cross-compilation:
    # - HTTP/WebSocket: require libsoup (no mingw package)
    # - Stdio: requires gwin32inputstream.h (not in mingw-glib2 headers)
    EXCLUDED_SRCS := $(SRCDIR)/mcp-http-transport.c $(SRCDIR)/mcp-websocket-transport.c \
                     $(SRCDIR)/mcp-stdio-transport.c
    EXCLUDED_TESTS := $(TESTDIR)/test-http-transport.c $(TESTDIR)/test-websocket-transport.c \
                      $(TESTDIR)/test-transport-mock.c $(TESTDIR)/test-integration.c
    PLATFORM_CFLAGS := -DMCP_NO_LIBSOUP -DMCP_NO_STDIO_TRANSPORT
else
    # Linux: SO with versioning, full feature set
    PKG_DEPS    := glib-2.0 gobject-2.0 gio-2.0 gio-unix-2.0 json-glib-1.0 libsoup-3.0 libdex-1
    LIB_STATIC  := lib$(PROJECT)-$(API_VERSION).a
    LIB_SHARED  := lib$(PROJECT)-$(API_VERSION).so
    LIB_SHARED_VERSION := lib$(PROJECT)-$(API_VERSION).so.$(VERSION)
    LIB_SHARED_SONAME  := lib$(PROJECT)-$(API_VERSION).so.1
    EXE_EXT     :=
    SHARED_LDFLAGS = -shared -Wl,-soname,$(LIB_SHARED_SONAME)
    EXCLUDED_SRCS :=
    EXCLUDED_TESTS :=
    PLATFORM_CFLAGS :=
endif

#=============================================================================
# Compiler/Linker Flags
#=============================================================================

CSTD        := -std=gnu89
WARNINGS    := -Wall -Wextra -Wno-unused-parameter
DEBUG       := -g -O0
PKG_CFLAGS  := $(shell $(PKG_CONFIG) --cflags $(PKG_DEPS))
PKG_LIBS    := $(shell $(PKG_CONFIG) --libs $(PKG_DEPS))

CFLAGS      := $(CSTD) $(WARNINGS) $(DEBUG) -fPIC -I./$(SRCDIR) $(SYSROOT_FLAGS) $(PLATFORM_CFLAGS) $(PKG_CFLAGS)
LDFLAGS     := $(SYSROOT_FLAGS) $(PKG_LIBS)

#=============================================================================
# Library Naming (for compatibility)
#=============================================================================

LIB_NAME    := lib$(PROJECT)-$(API_VERSION)
LIB_SO      := $(LIB_SHARED)
LIB_A       := $(LIB_STATIC)

#=============================================================================
# GObject Introspection (Linux native only)
#=============================================================================

GIR_SCANNER := g-ir-scanner
GIR_COMPILER:= g-ir-compiler
GIR_FILE    := Mcp-$(API_VERSION).gir
TYPELIB     := Mcp-$(API_VERSION).typelib

#=============================================================================
# Source Files
#=============================================================================

ALL_SRCS    := $(wildcard $(SRCDIR)/*.c)
SRCS        := $(filter-out $(EXCLUDED_SRCS),$(ALL_SRCS))
OBJS        := $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
DEPS        := $(OBJS:.o=.d)
HDRS        := $(wildcard $(SRCDIR)/*.h)

#=============================================================================
# Test Configuration
#=============================================================================

ALL_TEST_SRCS := $(wildcard $(TESTDIR)/*.c)
TEST_SRCS   := $(filter-out $(EXCLUDED_TESTS),$(ALL_TEST_SRCS))
TEST_BINS   := $(TEST_SRCS:$(TESTDIR)/%.c=$(BUILDDIR)/%$(EXE_EXT))

#=============================================================================
# Example Configuration
#=============================================================================

EXAMPLE_SRCS := $(wildcard $(EXAMPLESDIR)/*.c)
EXAMPLE_BINS := $(EXAMPLE_SRCS:$(EXAMPLESDIR)/%.c=$(BUILDDIR)/%$(EXE_EXT))

#=============================================================================
# Platform Marker (Auto-Clean on Platform Switch)
#=============================================================================

PLATFORM_MARKER := $(BUILDDIR)/.platform

#=============================================================================
# Install Configuration
#=============================================================================

PREFIX      ?= /usr/local
LIBDIR      := $(PREFIX)/lib
INCLUDEDIR  := $(PREFIX)/include/$(PROJECT)-$(API_VERSION)
PKGCONFIGDIR:= $(LIBDIR)/pkgconfig
GIRDIR      := $(PREFIX)/share/gir-1.0
TYPELIBDIR  := $(LIBDIR)/girepository-1.0

#=============================================================================
# Targets
#=============================================================================

.PHONY: all clean test examples tools gir install uninstall docs info platform-check

all: platform-check $(BUILDDIR)/$(LIB_SHARED) $(BUILDDIR)/$(LIB_STATIC) $(BUILDDIR)/$(PROJECT)-$(API_VERSION).pc

#=============================================================================
# Platform Check (Auto-Clean on Platform Switch)
#=============================================================================

platform-check:
	@mkdir -p $(BUILDDIR)
	@if [ -f "$(PLATFORM_MARKER)" ] && \
	   [ "$$(cat $(PLATFORM_MARKER))" != "$(TARGET_PLATFORM)" ]; then \
		echo "Platform changed from $$(cat $(PLATFORM_MARKER)) to $(TARGET_PLATFORM), cleaning..."; \
		rm -rf $(BUILDDIR)/*; \
		mkdir -p $(BUILDDIR); \
	fi
	@echo "$(TARGET_PLATFORM)" > $(PLATFORM_MARKER)

#=============================================================================
# Library Targets
#=============================================================================

# Shared library (platform-specific)
ifeq ($(TARGET_PLATFORM),windows)
$(BUILDDIR)/$(LIB_SHARED): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(SHARED_LDFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built: $(LIB_SHARED) and $(LIB_IMPORT)"
else
$(BUILDDIR)/$(LIB_SHARED): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(SHARED_LDFLAGS) -o $(BUILDDIR)/$(LIB_SHARED_VERSION) $^ $(LDFLAGS)
	ln -sf $(LIB_SHARED_VERSION) $(BUILDDIR)/$(LIB_SHARED_SONAME)
	ln -sf $(LIB_SHARED_SONAME) $(BUILDDIR)/$(LIB_SHARED)
	@echo "Built: $(LIB_SHARED_VERSION) with symlinks"
endif

# Static library
$(BUILDDIR)/$(LIB_STATIC): $(OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^
	$(RANLIB) $@

#=============================================================================
# Object File Rules
#=============================================================================

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEPS)

#=============================================================================
# Test Targets
#=============================================================================

test: platform-check $(TEST_BINS)
ifeq ($(TARGET_PLATFORM),windows)
	@echo "Cross-compiled tests cannot be run on Linux. Use Wine or copy to Windows."
	@echo "Test binaries built in $(BUILDDIR)/"
	@echo "Example: wine $(BUILDDIR)/test-enums.exe"
else ifeq ($(CROSS),aarch64-linux-gnu)
	@echo "Running ARM64 tests via QEMU (requires qemu-user-static)..."
	@for t in $(TEST_BINS); do \
		echo "  Running $$t"; \
		qemu-aarch64 -L $(ARM64_SYSROOT) $$t || exit 1; \
	done
	@echo "All tests passed."
else
	@echo "Running tests..."
	@for t in $(TEST_BINS); do \
		echo "  Running $$t"; \
		$$t || exit 1; \
	done
	@echo "All tests passed."
endif

# Special rule for test-cli-common (requires mcp-common.o from tools/)
$(BUILDDIR)/test-cli-common$(EXE_EXT): $(TESTDIR)/test-cli-common.c $(BUILDDIR)/mcp-common.o $(BUILDDIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(TOOLSDIR) -o $@ $< $(BUILDDIR)/mcp-common.o \
		-L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS) \
		-Wl,-rpath,$(CURDIR)/$(BUILDDIR)

# Generic pattern rule for other tests
$(BUILDDIR)/test-%$(EXE_EXT): $(TESTDIR)/test-%.c $(BUILDDIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
ifeq ($(TARGET_PLATFORM),windows)
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS)
else
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS) \
		-Wl,-rpath,$(CURDIR)/$(BUILDDIR)
endif

#=============================================================================
# Example Targets
#=============================================================================

examples: platform-check $(EXAMPLE_BINS)

$(BUILDDIR)/%$(EXE_EXT): $(EXAMPLESDIR)/%.c $(BUILDDIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
ifeq ($(TARGET_PLATFORM),windows)
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS) -lm
else
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS) -lm \
		-Wl,-rpath,$(CURDIR)/$(BUILDDIR)
endif

#=============================================================================
# CLI Tools Configuration
#=============================================================================

TOOLSDIR    := tools

# mcp-shell requires readline (check if available)
READLINE_CFLAGS := $(shell $(PKG_CONFIG) --cflags readline 2>/dev/null)
READLINE_LIBS   := $(shell $(PKG_CONFIG) --libs readline 2>/dev/null)
HAVE_READLINE   := $(shell $(PKG_CONFIG) --exists readline 2>/dev/null && echo yes || echo no)

#=============================================================================
# CLI Tools Targets
#=============================================================================

ifeq ($(TARGET_PLATFORM),windows)
tools:
	@echo "CLI tools are not available for Windows cross-compilation"
	@echo "(requires stdio transport which is excluded)"
else

ifeq ($(HAVE_READLINE),yes)
SHELL_TARGET := $(BUILDDIR)/mcp-shell
else
SHELL_TARGET :=
endif

tools: platform-check $(BUILDDIR)/mcp-inspect $(BUILDDIR)/mcp-call \
       $(BUILDDIR)/mcp-read $(BUILDDIR)/mcp-prompt $(SHELL_TARGET)
ifeq ($(HAVE_READLINE),no)
	@echo ""
	@echo "Note: mcp-shell was not built (readline-devel not found)"
	@echo "Install readline-devel to build the interactive shell:"
	@echo "  sudo dnf install readline-devel"
endif

# Common object file (shared utilities)
$(BUILDDIR)/mcp-common.o: $(TOOLSDIR)/mcp-common.c $(TOOLSDIR)/mcp-common.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(TOOLSDIR) -c -o $@ $<

# mcp-inspect: Server inspection tool
$(BUILDDIR)/mcp-inspect: $(TOOLSDIR)/mcp-inspect.c $(BUILDDIR)/mcp-common.o $(BUILDDIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(TOOLSDIR) -o $@ $< $(BUILDDIR)/mcp-common.o \
		-L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS) \
		-Wl,-rpath,$(CURDIR)/$(BUILDDIR)

# mcp-call: Tool invocation
$(BUILDDIR)/mcp-call: $(TOOLSDIR)/mcp-call.c $(BUILDDIR)/mcp-common.o $(BUILDDIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(TOOLSDIR) -o $@ $< $(BUILDDIR)/mcp-common.o \
		-L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS) \
		-Wl,-rpath,$(CURDIR)/$(BUILDDIR)

# mcp-read: Resource reader
$(BUILDDIR)/mcp-read: $(TOOLSDIR)/mcp-read.c $(BUILDDIR)/mcp-common.o $(BUILDDIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(TOOLSDIR) -o $@ $< $(BUILDDIR)/mcp-common.o \
		-L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS) \
		-Wl,-rpath,$(CURDIR)/$(BUILDDIR)

# mcp-prompt: Prompt retrieval
$(BUILDDIR)/mcp-prompt: $(TOOLSDIR)/mcp-prompt.c $(BUILDDIR)/mcp-common.o $(BUILDDIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(TOOLSDIR) -o $@ $< $(BUILDDIR)/mcp-common.o \
		-L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS) \
		-Wl,-rpath,$(CURDIR)/$(BUILDDIR)

# mcp-shell: Interactive REPL (requires readline)
$(BUILDDIR)/mcp-shell: $(TOOLSDIR)/mcp-shell.c $(BUILDDIR)/mcp-common.o $(BUILDDIR)/$(LIB_SHARED)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(TOOLSDIR) $(READLINE_CFLAGS) -o $@ $< $(BUILDDIR)/mcp-common.o \
		-L$(BUILDDIR) -l$(PROJECT)-$(API_VERSION) $(LDFLAGS) $(READLINE_LIBS) \
		-Wl,-rpath,$(CURDIR)/$(BUILDDIR)

endif

#=============================================================================
# pkg-config File
#=============================================================================

$(BUILDDIR)/$(PROJECT)-$(API_VERSION).pc: $(PROJECT)-$(API_VERSION).pc.in
	@mkdir -p $(dir $@)
	sed -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@VERSION@|$(VERSION)|g' \
	    -e 's|@API_VERSION@|$(API_VERSION)|g' \
	    $< > $@

#=============================================================================
# GObject Introspection (Linux native only)
#=============================================================================

gir: $(BUILDDIR)/$(GIR_FILE) $(BUILDDIR)/$(TYPELIB)

ifneq ($(TARGET_PLATFORM),linux)
$(BUILDDIR)/$(GIR_FILE):
	@echo "GIR generation only available for native Linux builds"
	@false

$(BUILDDIR)/$(TYPELIB):
	@echo "Typelib generation only available for native Linux builds"
	@false
else
ifneq ($(CROSS),)
$(BUILDDIR)/$(GIR_FILE):
	@echo "GIR generation only available for native (non-cross) Linux builds"
	@false

$(BUILDDIR)/$(TYPELIB):
	@echo "Typelib generation only available for native (non-cross) Linux builds"
	@false
else
$(BUILDDIR)/$(GIR_FILE): $(BUILDDIR)/$(LIB_SHARED) $(HDRS) $(SRCS)
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
		-I./$(SRCDIR) \
		$(HDRS) $(SRCS)

$(BUILDDIR)/$(TYPELIB): $(BUILDDIR)/$(GIR_FILE)
	$(GIR_COMPILER) $< -o $@
endif
endif

#=============================================================================
# Installation (Linux native only)
#=============================================================================

install: all tools
ifeq ($(TARGET_PLATFORM),windows)
	@echo "Install target is for native Linux builds only."
	@echo "For Windows, copy the DLL and headers manually."
else ifneq ($(CROSS),)
	@echo "Install target is for native (non-cross) builds only."
else
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -d $(DESTDIR)$(PKGCONFIGDIR)
	install -d $(DESTDIR)$(GIRDIR)
	install -d $(DESTDIR)$(TYPELIBDIR)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BUILDDIR)/$(LIB_SHARED_VERSION) $(DESTDIR)$(LIBDIR)/
	install -m 644 $(BUILDDIR)/$(LIB_STATIC) $(DESTDIR)$(LIBDIR)/
	ln -sf $(LIB_SHARED_VERSION) $(DESTDIR)$(LIBDIR)/$(LIB_SHARED_SONAME)
	ln -sf $(LIB_SHARED_SONAME) $(DESTDIR)$(LIBDIR)/$(LIB_SHARED)
	install -m 644 $(HDRS) $(DESTDIR)$(INCLUDEDIR)/
	install -m 644 $(BUILDDIR)/$(PROJECT)-$(API_VERSION).pc $(DESTDIR)$(PKGCONFIGDIR)/
	install -m 755 $(BUILDDIR)/mcp-inspect $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(BUILDDIR)/mcp-call $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(BUILDDIR)/mcp-read $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(BUILDDIR)/mcp-prompt $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(BUILDDIR)/mcp-shell $(DESTDIR)$(PREFIX)/bin/
	if [ -f $(BUILDDIR)/$(GIR_FILE) ]; then \
		install -m 644 $(BUILDDIR)/$(GIR_FILE) $(DESTDIR)$(GIRDIR)/; \
	fi
	if [ -f $(BUILDDIR)/$(TYPELIB) ]; then \
		install -m 644 $(BUILDDIR)/$(TYPELIB) $(DESTDIR)$(TYPELIBDIR)/; \
	fi
	ldconfig $(DESTDIR)$(LIBDIR) || true
endif

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_SHARED)*
	rm -f $(DESTDIR)$(LIBDIR)/$(LIB_STATIC)
	rm -rf $(DESTDIR)$(INCLUDEDIR)
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/$(PROJECT)-$(API_VERSION).pc
	rm -f $(DESTDIR)$(GIRDIR)/$(GIR_FILE)
	rm -f $(DESTDIR)$(TYPELIBDIR)/$(TYPELIB)
	rm -f $(DESTDIR)$(PREFIX)/bin/mcp-inspect
	rm -f $(DESTDIR)$(PREFIX)/bin/mcp-call
	rm -f $(DESTDIR)$(PREFIX)/bin/mcp-read
	rm -f $(DESTDIR)$(PREFIX)/bin/mcp-prompt
	rm -f $(DESTDIR)$(PREFIX)/bin/mcp-shell

#=============================================================================
# Documentation (placeholder)
#=============================================================================

docs:
	@echo "Documentation generation not yet implemented"

#=============================================================================
# Clean
#=============================================================================

clean:
	rm -rf $(BUILDDIR)

#=============================================================================
# Debug Info
#=============================================================================

info:
	@echo "=== Build Configuration ==="
	@echo "TARGET_PLATFORM: $(TARGET_PLATFORM)"
	@echo "TARGET_ARCH:     $(TARGET_ARCH)"
	@echo "CROSS:           $(CROSS)"
	@echo "CC:              $(CC)"
	@echo "AR:              $(AR)"
	@echo "PKG_CONFIG:      $(PKG_CONFIG)"
ifeq ($(CROSS),aarch64-linux-gnu)
	@echo "ARM64_SYSROOT:   $(ARM64_SYSROOT)"
endif
	@echo ""
	@echo "=== Dependencies ==="
	@echo "PKG_DEPS:        $(PKG_DEPS)"
	@echo ""
	@echo "=== Output Files ==="
	@echo "LIB_STATIC:      $(LIB_STATIC)"
	@echo "LIB_SHARED:      $(LIB_SHARED)"
ifeq ($(TARGET_PLATFORM),windows)
	@echo "LIB_IMPORT:      $(LIB_IMPORT)"
else
	@echo "LIB_SHARED_VERSION: $(LIB_SHARED_VERSION)"
	@echo "LIB_SHARED_SONAME:  $(LIB_SHARED_SONAME)"
endif
	@echo "EXE_EXT:         '$(EXE_EXT)'"
	@echo ""
	@echo "=== Source Files ==="
	@echo "SRCS:            $(SRCS)"
	@echo "OBJS:            $(OBJS)"
	@echo "HDRS:            $(HDRS)"
	@echo "TEST_BINS:       $(TEST_BINS)"
	@echo ""
	@echo "=== Directories ==="
	@echo "BUILDDIR:        $(BUILDDIR)"
	@echo "PREFIX:          $(PREFIX)"
	@echo ""
	@echo "=== Flags ==="
	@echo "CFLAGS:          $(CFLAGS)"
	@echo "LDFLAGS:         $(LDFLAGS)"
