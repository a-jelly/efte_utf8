# Building eFTE UTF-8

## Prerequisites

A C++11 compiler, CMake 2.8+, and the following libraries:

### Required

| Library | Package (Debian/Ubuntu) | Package (Arch) | Package (Fedora) |
|---------|------------------------|----------------|-------------------|
| **PCRE2** (8-bit) | `libpcre2-dev` | `pcre2` | `pcre2-devel` |
| **libyaml** | `libyaml-dev` | `libyaml` | `libyaml-devel` |

These are used by the Sublime-syntax highlighting engine.

### Optional (but recommended)

| Library | What for | Package (Debian/Ubuntu) |
|---------|----------|------------------------|
| X11 + Xpm | GUI version (`efte`) | `libx11-dev libxpm-dev` |
| Xft | Anti-aliased fonts in GUI | `libxft-dev` |
| ncursesw | Terminal version (`nefte`) | `libncursesw5-dev` |
| GPM | Mouse in console | `libgpm-dev` |

### Quick install (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake \
    libpcre2-dev libyaml-dev \
    libx11-dev libxpm-dev libxft-dev \
    libncursesw5-dev libgpm-dev
```

### Quick install (Arch)

```bash
sudo pacman -S base-devel cmake pcre2 libyaml \
    libx11 libxpm libxft ncurses gpm
```

## Build

```bash
cd src
cmake .
make -j$(nproc)
```

This produces up to three executables:

| Binary | Description |
|--------|-------------|
| `efte` | X11 GUI version |
| `nefte` | ncurses terminal version |
| `vefte` | Plain VT terminal version |

## Install

```bash
sudo make install
```

Installs binaries to `/usr/local/bin` by default. Override with:

```bash
cmake -DCMAKE_INSTALL_PREFIX=/opt/efte .
```

## Out-of-source build

```bash
mkdir build && cd build
cmake ../src
make -j$(nproc)
```

## Troubleshooting

**PCRE2 or libyaml not found** — install the `-dev` / `-devel` packages.
CMake tries `pkg-config` first, then falls back to `find_library`.

**No X11** — `efte` (GUI) won't be built; `nefte` and `vefte` still will.

**No ncursesw** — `nefte` (terminal) won't be built; `efte` and `vefte` still will.
