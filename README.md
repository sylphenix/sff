# sff

`sff` (simple file finder) is a simple, fast, and feature-rich terminal file
manager inspired by `nnn` and guided by the suckless philosophy. It aims to 
provide a reliable, efficient, and user-friendly file management experience with 
high extensibility. `sff` is fully compatible with POSIX-compliant systems. It 
has been extensively tested on GNU/Linux and FreeBSD.


## Features

- POSIX-compliant and highly optimized
- Fast startup and low memory footprint
- Extensible with shell scripts
- Customizable detail columns
- Type-to-navigate
- Advanced search with `find`
- Fast file search with `fzf`
- Convenient temporary sudo mode
- Undo/Redo for the last file operation
- Batch file and directory creation
- Batch rename
- Multi-tab support, cross-directory selection
- Extract, list, create archives
- ... and more!


## Installation

### Install from binary packages
1. [Download](https://github.com/sylphenix/sff/releases) the appropriate package for your system.

2. Navigate to the directory where the downloaded package is located.

3. Install the package using the package manager specific to your system.
- Debian/Ubuntu:
   ```
   sudo apt install sff_xxx_amd64.deb
   ```
- Arch Linux:
   ```
   sudo pacman -U sff-xxx-x86_64.pkg.tar.zst
   ```
- Fedora:
   ```
   sudo dnf install sff-xxx.86_64.rpm
   ```
- FreeBSD
   ```
   sudo pkg add sff-xxx.pkg
   ```

### Install from source
For Linux users, ensure that `gcc` and `make` are already installed on your system.  

1. Install dependencies. (Linux only)
- Debian/Ubuntu:
   ```
   sudo apt install libncurses-dev
   ```
- Arch Linux:
   ```
   sudo pacman -S ncurses
   ```
   *Note: On Arch Linux, this step is usually unnecessary since ncurses is part of the base installation.*

- Fedora:
   ```
   sudo dnf install ncurses-devel
   ```
- openSUSE:
   ```
   sudo zypper install ncurses-devel
   [ -e /usr/include/ncursesw/curses.h ] && sudo ln -fs ncursesw/curses.h /usr/include/curses.h
   ```

2. Download or clone the source code repository.

3. Navigate to the root directory of the project.

4. Run the following command to build and install sff:
- Linux:
   ```
   sudo make install PREFIX=/usr
   ```
- FreeBSD
   ```
   sudo make install
   ```

## Usage

Simply run `sff` to start the application from the current directory.

Run `sff -h` to see command line options.

While sff is running:
- Press `?` or `F1` to see the list of key bindings for built-in functions.
- Press `alt`+`/` to see the list of key bindings for extension functions.
- Press `Q` to quit sff.

For more details, run `man sff` to see the documentation, or visit the [wiki](https://github.com/sylphenix/sff/wiki) for useful tips and tricks.


## Philosophy
sff is built on the belief that simplicity ensures reliability. It follows a minimalist design, divided into two parts: the core program and the extension script. The core program is a lightweight file browser and selector, sticking to features that are simple, necessary, and straightforward to implement. The extension script, a POSIX-compliant shell script, handles file operations such as copying, moving, and deleting. This modular design allows users to easily customize or extend functionality while keeping the core simple and efficient.

## License

sff is released under the 2-Clause BSD License. See the LICENSE file for more details.

## Acknowledgements

Special thanks to [nnn](https://github.com/jarun/nnn) and [suckless.org](https://suckless.org).
