# sff

sff (simple file finder) is a simple, fast, and feature-rich terminal file manager inspired by nnn and guided by the suckless philosophy.
It aims to provide a reliable, efficient, and user-friendly file management experience with high extensibility.
sff is fully compatible with POSIX-compliant systems. It has been extensively tested on GNU/Linux and FreeBSD.


## Features

- POSIX-compliant and highly optimized
- Fast startup and low memory footprint
- Extensible with shell scripts
- Customizable detail columns
- Type-to-navigate
- Advanced search via 'find'
- Fast file search via 'fzf'
- Convenient temporary sudo mode
- Undo/Redo for the last file operation
- Batch file and directory creation
- Batch rename
- Multi-tab support, cross-directory selection
- Extract and create archives
- ... and more!


## Dependencies

Core Dependencies:
- **libc, curses (wide-character)**: Essential runtime libraries
- **coreutils, POSIX shell, findutils, sed**: For file operations
- **vi/vim**: Default text editor
- **sudo**: For sudo mode
- **xdg-utils**: File opening with default application

*Core dependencies are part of the base system in most environments and generally don't require manual installation.*

Plugin Dependencies:
| Plugin   | Dependencies                            | Notes                               |
|----------|-----------------------------------------|-------------------------------------|
| archive  | tar, gzip, bzip2, xz, 7zip              | Archive handling                    |
| fzf-find | fzf                                     | Fuzzy file search                   |
| preview  | chafa, poppler-utils, ffmpegthumbnailer | Image, PDF, video thumbnail preview |

You can install all dependencies using the following commands:
- Debian/Ubuntu:
   ```
   sudo apt install 7zip fzf chafa poppler-utils ffmpegthumbnailer
   ```
- Arch Linux:
   ```
   sudo pacman -S 7zip fzf chafa poppler ffmpegthumbnailer
   ```
- Fedora:
   ```
   sudo dnf install p7zip fzf chafa poppler-utils ffmpegthumbnailer
   ```
- FreeBSD:
   ```
   sudo pkg install 7-zip fzf chafa poppler-utils ffmpegthumbnailer
   ```
- macOS:
   ```
   brew install ncurses sevenzip fzf chafa poppler ffmpegthumbnailer
   ```


## Installation

### Install from packages

- Packaging status:

  [![Packaging status](https://repology.org/badge/vertical-allrepos/sff.svg)](https://repology.org/project/sff/versions)


- Official packages are available from [OpenBuildService](https://software.opensuse.org/download.html?project=home%3Asylphenix%3Asff&package=sff).


### Build and install from source

0. For Linux users, ensure that a C compiler, make utility, and the ncurses headers are installed. You can install them using the following commands:
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

3. Build and install sff:
   ```
   sudo make install
   ```

   By default, files are installed under `/usr/local`. You can use the `PREFIX` variable to change this:
   ```
   sudo make install PREFIX=/usr
   ```

   If you used `PREFIX` during installation, you must specify the same `PREFIX` when uninstalling:
   ```
   sudo make uninstall PREFIX=/usr
   ```

   **Gentoo note:** To resolve dependencies, explicitly link both `libncursesw` and `libtinfow` libraries:
   ```
   sudo make install LDFLAGS="-lncursesw -ltinfow"
   ```
   
   **macOS note:** See the [wiki](https://codeberg.org/sylphenix/sff/wiki#macos-specific-notes) for details.


## Usage

Simply run `sff` to start the program from the current directory.

While sff is running:
- Press `?` or `F1` to see the list of key bindings for built-in functions.
- Press `Alt`+`/` to see the list of key bindings for extension functions and plugins.
- Press `Q` to quit sff.

For more details, run `man sff` to see the documentation, or visit the [wiki](https://codeberg.org/sylphenix/sff/wiki/Home) for useful tips and tricks.


## Philosophy

sff is built on the belief that simplicity ensures reliability.
It follows a minimalist design, divided into two parts: the core program and the extension script.
The core program is a lightweight file browser and selector, sticking to features that are simple, necessary, and straightforward to implement.
The extension script, a POSIX-compliant shell script, handles file operations such as copying, moving, and deleting.
This modular design allows users to easily customize or extend functionality while keeping the core simple and efficient.


## License

sff is released under the 2-Clause BSD License. See the LICENSE file for more details.


## Acknowledgements

Special thanks to [nnn](https://github.com/jarun/nnn) and [suckless.org](https://suckless.org).
