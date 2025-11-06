# QyzylPM - Qyzyl Package Manager

QPM is a simple command-line package manager written in C.
It installs software from custom mirrors using `.qbf` build files.

---

## Features

- Lightweight, no dependencies except libcurl
- Configurable mirrors (`/etc/qpm.conf`)
- Simple `.qbf` package definition format

---

## Requirements

- GCC or Clang
- libcurl development package

## Installation
```bash
# Compile
make

# Install to system
sudo make install

# Install into custom root
make DESTDIR=/mnt/lfs install

# Remove
sudo make uninstall
```
