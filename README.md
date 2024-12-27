# sff

`sff` (simple file finder) is a simple, fast, and feature-rich terminal file
manager inspired by `nnn` and guided by the suckless philosophy. It aims to
provide a reliable, efficient, user-friendly, and highly extensible file 
management experience. `sff` is designed to be fully compatible with
POSIX-compliant systems. It has been extensively tested on GNU/Linux and FreeBSD.


## Features

- POSIX-compliant and highly optimized
- Fast startup and low memory footprint
- Extendable with shell scripts
- Customizable detail columns
- Quick find for navigation
- Advanced searching with `find`
- Rapid file search with `fzf`
- Temporary sudo mode
- Undo/Redo for the last file operation
- Batch creation of files and directories
- Batch rename
- Multi-tab support, cross-directory selection
- Extract, list, create archives
- ... and more!


## Installation

### Install from source
For Linux users, ensure that `gcc` and `make` are already installed on your system.  

1. Install dependencies. (Linux only)
- Debian/Ubuntu:
   ```bash
   sudo apt install libncurses-dev
   ```
- Arch Linux:
   ```bash
   sudo pacman -S ncurses
   ```
- Fedora:
   ```bash
   sudo dnf install ncurses-devel
   ```

2. Download or clone the source code repository.

3. Navigate to the root directory of the project.

4. Run the following command:
   ```bash
   make
   ```

5. Optionally, install it system-wide:
   ```bash
   sudo make install
   ```

## Configuration

There is no configuration file. sff is customized by edit config.h and (re)compiling
the source code. This keeps it simple and fast.

## Usage

To run sff from its build directory, use the following command:
```bash
./sff
```

If sff is installed system-wide, you can simply run:
```bash
sff
```

## License

sff is released under the 2-Clause BSD License. See the LICENSE file for more details.

## Acknowledgements

Thanks to [nnn](https://github.com/jarun/nnn) and [suckless.org](https://suckless.org).
