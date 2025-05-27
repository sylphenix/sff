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

### Dependencies

| Library/Package                                 | Install?  | Notes                                     |
|-------------------------------------------------|-----------|-------------------------------------------|
| libc, curses (wide character support)           | Required* | Essential runtime libraries               |
| coreutils (Linux), findutils (Linux), sed, file | Required* | File operations                           |
| vi/vim                                          | Required* | Default text editor                       |
| sudo                                            | Optional* | Sudo mode                                 |
| xdg-utils                                       | Optional* | File opening via default applications     |
| tar, gzip, bzip2, xz, 7zip                      | Optional  | Archive handling (archive plugin)         |
| fzf                                             | Optional  | File search with fzf (fzf-find plugin)    |
| chafa                                           | Optional  | Image preview (preview plugin)            |
| poppler-utils                                   | Optional  | PDF preview (preview plugin)              |
| ffmpegthumbnailer                               | Optional  | Video thumbnail preview (preview plugin)  |

_* These dependencies are part of the base system in most environments and generally don't require manual installation._


You can install all dependencies using the following commands:
- Debian/Ubuntu:
   ```
   sudo apt install 7zip fzf chafa poppler-utils ffmpegthumbnailer
   ```
- Arch Linux:
   ```
   sudo pacman -S 7zip fzf chafa poppler-utils ffmpegthumbnailer
   ```
- Fedora:
   ```
   sudo dnf install 7zip fzf chafa poppler-utils ffmpegthumbnailer
   ```
- FreeBSD:
   ```
   sudo pkg install 7-zip fzf chafa poppler-utils ffmpegthumbnailer
   ```


### Install from binary packages
1. [Download](https://codeberg.org/sylphenix/sff/releases) the appropriate package for your system.

2. Install the package using the package manager specific to your system.
- Debian/Ubuntu:
   ```
   sudo apt install /PATH/TO/sff_VERSION_amd64.deb
   ```
- Arch Linux:
   ```
   sudo pacman -U /PATH/TO/sff-VERSION-x86_64.pkg.tar.zst
   ```
- Fedora:
   ```
   sudo dnf install /PATH/TO/sff-VERSION.86_64.rpm
   ```

### Build and install from source
0. For Linux users, ensure that a C compiler, build tools, and the ncurses header files are installed. You can install them using the following commands:
- Debian/Ubuntu:
   ```
   sudo apt install gcc make libncurses-dev
   ```
- Arch Linux:
   ```
   sudo pacman -S gcc make ncurses
   ```
- Fedora:
   ```
   sudo dnf install gcc make ncurses-devel
   ```

1. [Download](https://codeberg.org/sylphenix/sff/releases) and extract the latest release, or clone the repository to get the development version.

2. Change to the root directory of the project.

3. Run the following command to build and install sff:
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

While sff is running:
- Press `?` or `F1` to see the list of key bindings for built-in functions.
- Press `alt`+`/` to see the list of key bindings for extension functions and plugins.
- Press `Q` to quit sff.

For more details, run `man sff` to see the documentation, or visit the [wiki](https://codeberg.org/sylphenix/sff/wiki/Home) for useful tips and tricks.


## Philosophy
sff is built on the belief that simplicity ensures reliability. It follows a minimalist design, divided into two parts: the core program and the extension script. The core program is a lightweight file browser and selector, sticking to features that are simple, necessary, and straightforward to implement. The extension script, a POSIX-compliant shell script, handles file operations such as copying, moving, and deleting. This modular design allows users to easily customize or extend functionality while keeping the core simple and efficient.


## License

sff is released under the 2-Clause BSD License. See the LICENSE file for more details.

## Acknowledgements

Special thanks to [nnn](https://github.com/jarun/nnn) and [suckless.org](https://suckless.org).
