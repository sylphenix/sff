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
   ```
   sudo apt install libncurses-dev
   ```
- Arch Linux:
   ```
   sudo pacman -S ncurses
   ```
   *Note: Typically, you can skip this step on Arch, as ncurses is already included in the base installation package.*

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
   ```
   sudo make install
   ```

## Usage

Simply run `sff` to start the application from the current directory.

Run `sff -h` to see command line options.

While sff is running:
- Press `?` or `F1` to see the list of keybinds for built-in functions.
- Press `Alt`+`/` to see the list of keybinds for external functions.
- Press `Q` to quit sff.

For more details, run `man sff` to see the documentation.

## License

sff is released under the 2-Clause BSD License. See the LICENSE file for more details.

## Acknowledgements

Thanks to [nnn](https://github.com/jarun/nnn) and [suckless.org](https://suckless.org).
