# Building mcp-glib

## Requirements

### Compiler

- GCC with gnu89 support (tested with GCC 14+)

### Dependencies

| Package | Minimum Version | Description |
|---------|-----------------|-------------|
| glib-2.0 | 2.70 | Core utilities |
| gobject-2.0 | 2.70 | Object system |
| gio-2.0 | 2.70 | I/O library |
| gio-unix-2.0 | 2.70 | Unix-specific I/O (Linux only) |
| json-glib-1.0 | 1.6 | JSON parsing |
| libsoup-3.0 | 3.0 | HTTP library (optional for cross-builds) |
| libdex-1 | 0.4 | Async futures (optional for cross-builds) |

### Optional

| Package | Description |
|---------|-------------|
| g-ir-scanner | GObject Introspection scanner (for bindings) |
| g-ir-compiler | GObject Introspection compiler (for bindings) |

## Installing Dependencies

### Fedora 43+

```bash
sudo dnf install gcc make pkgconfig \
    glib2-devel json-glib-devel libsoup3-devel libdex-devel \
    gobject-introspection-devel
```

### Ubuntu 24.04+

```bash
sudo apt install gcc make pkg-config \
    libglib2.0-dev libjson-glib-dev libsoup-3.0-dev libdex-1-dev \
    gobject-introspection
```

### Arch Linux

```bash
sudo pacman -S gcc make pkgconf \
    glib2 json-glib libsoup3 libdex \
    gobject-introspection
```

## Cross-Compilation

mcp-glib supports cross-compilation for Windows and Linux ARM64.

### Supported Targets

| Target | Command | Output |
|--------|---------|--------|
| Windows x64 | `make WINDOWS=1` | `mcp-glib-1.0.dll` |
| Linux ARM64 | `make LINUX_ARM64=1` | `libmcp-glib-1.0.so` |
| Custom | `make CROSS=<prefix>` | Platform-specific |

### Windows Cross-Compilation (mingw)

#### Requirements

```bash
# Fedora
sudo dnf install mingw64-gcc mingw64-glib2 mingw64-json-glib mingw64-pkg-config
```

#### Building

```bash
make WINDOWS=1
```

Creates:
- `build/mcp-glib-1.0.dll` - Windows DLL
- `build/libmcp-glib-1.0.dll.a` - Import library
- `build/libmcp-glib-1.0.a` - Static library

#### Limitations

Due to Fedora mingw package limitations, some features are excluded:

| Feature | Status | Reason |
|---------|--------|--------|
| Core MCP types | Available | - |
| Stdio transport | Excluded | `gwin32inputstream.h` not in mingw-glib2 |
| HTTP transport | Excluded | No `mingw64-libsoup3` package |
| WebSocket transport | Excluded | No `mingw64-libsoup3` package |
| GIR generation | Excluded | Cross-compilation not supported |

For full Windows support with all transports, build natively on Windows with MSYS2.

### ARM64 Linux Cross-Compilation

#### Requirements

```bash
# Fedora - Install cross-compiler and base sysroot
sudo dnf install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu sysroot-aarch64-fc41-glibc
```

#### Sysroot Setup

The base Fedora sysroot does not include the required libraries. Use the provided script to download and install ARM64 packages:

```bash
# Add libraries to the Fedora sysroot (requires sudo)
sudo ./scripts/setup-arm64-sysroot.sh

# Or use a custom sysroot path
./scripts/setup-arm64-sysroot.sh ~/arm64-sysroot
```

The script downloads ARM64 packages for: glib2, json-glib, libsoup3, libdex, and all dependencies.

#### Building

```bash
make LINUX_ARM64=1
```

### Build Configuration

View current build configuration:

```bash
make info
```

### Platform Switch

The build system automatically cleans when switching platforms:

```bash
make WINDOWS=1    # Builds for Windows
make              # Auto-cleans, then builds for Linux
```

## Build Targets

### Default Build

Build the shared and static libraries:

```bash
make
```

This creates:
- `build/libmcp-glib-1.0.so` - Shared library
- `build/libmcp-glib-1.0.a` - Static library
- `build/mcp-glib-1.0.pc` - pkg-config file

### Running Tests

```bash
make test
```

Runs all GTest-based unit tests. Current test coverage:
- 13 test suites
- 170+ individual tests
- Unit tests, protocol tests, integration tests

### Building Examples

```bash
make examples
```

Builds example programs in `build/`:
- `simple-server` - Basic server example
- `simple-client` - Basic client example
- `calculator-server` - Calculator tools
- `filesystem-server` - File serving

### GObject Introspection

Generate GIR and typelib files for language bindings:

```bash
make gir
```

This creates:
- `build/Mcp-1.0.gir` - GIR XML file
- `build/Mcp-1.0.typelib` - Compiled typelib

### Installation

Install to system (default prefix `/usr/local`):

```bash
sudo make install
```

Custom prefix:

```bash
make install PREFIX=/opt/mcp-glib DESTDIR=/tmp/staging
```

Installed files:
- Libraries to `$(PREFIX)/lib/`
- Headers to `$(PREFIX)/include/mcp-glib-1.0/`
- pkg-config to `$(PREFIX)/lib/pkgconfig/`
- GIR to `$(PREFIX)/share/gir-1.0/`
- Typelib to `$(PREFIX)/lib/girepository-1.0/`

### Uninstall

```bash
sudo make uninstall
```

### Cleaning

```bash
make clean
```

## Using in Your Project

### With pkg-config

```bash
gcc $(pkg-config --cflags mcp-glib-1.0) -c myserver.c
gcc -o myserver myserver.o $(pkg-config --libs mcp-glib-1.0)
```

### In a Makefile

```makefile
CFLAGS  += $(shell pkg-config --cflags mcp-glib-1.0)
LDFLAGS += $(shell pkg-config --libs mcp-glib-1.0)

myserver: myserver.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
```

## Troubleshooting

### pkg-config can't find mcp-glib-1.0

Ensure the pkg-config path includes the installation directory:

```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

### GIR/typelib not found

For Python bindings, ensure the typelib path is set:

```bash
export GI_TYPELIB_PATH=/usr/local/lib/girepository-1.0:$GI_TYPELIB_PATH
```

### libmcp-glib-1.0.so not found at runtime

Update the library path or run ldconfig:

```bash
sudo ldconfig /usr/local/lib
```

Or set LD_LIBRARY_PATH:

```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```
