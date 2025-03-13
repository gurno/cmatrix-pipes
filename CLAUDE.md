# CMatrix Development Guide

## Build Commands
```bash
# Build using autoconf (recommended for Linux/mingw)
autoreconf -i  # skip if using released tarball
./configure
make
make install

# Build using CMake
mkdir -p build
cd build
cmake ..  # or cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
make install
```

## Code Style
- Use C89 standard with consistent 4-space indentation
- Follow existing naming conventions: snake_case for variables/functions
- Properly handle errors with c_die() function when appropriate
- Group related includes at the top of files
- Use consistent commenting style with block comments for functions
- Use ncurses library for terminal manipulation
- Ensure memory is properly allocated and freed
- Keep functions modular and focused on single responsibility
- Maintain compatibility with various systems (Linux, macOS, Windows)

## Project Structure
- Main program logic in cmatrix.c
- Configuration handled through autoconf or CMake
- Console font files: matrix.fnt, matrix.psf.gz
- X11 font file: mtx.pcf