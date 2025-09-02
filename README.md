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


## Installation

### Dependencies

| Library/Package                                 | Requirement | Notes                                       |
|-------------------------------------------------|-------------|---------------------------------------------|
| libc, curses (wide character support)           | Required*   | Essential runtime libraries                 |
| coreutils (Linux), findutils (Linux), sed, file | Required*   | For file operations                         |
| vi/vim                                          | Required*   | Default text editor                         |
| sudo                                            | Optional*   | For sudo mode                               |
| xdg-utils                                       | Optional*   | File opening with default application       |
| tar, gzip, bzip2, xz, 7zip                      | Optional    | Archive handling (archive plugin)           |
| fzf                                             | Optional    | Fuzzy file search (fzf-find plugin)         |
| chafa                                           | Optional    | Image preview (preview plugin)              |
| poppler-utils                                   | Optional    | PDF thumbnail generation (preview plugin)   |
| ffmpegthumbnailer                               | Optional    | Video thumbnail generation (preview plugin) |

_* These dependencies are part of the base system in most environments and generally don't require manual installation._

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

### Install from binary packages

#### Linux:

1. Download the appropriate package for your system from [OpenBuildService](https://software.opensuse.org/download.html?project=home%3Asylphenix%3Asff&package=sff).

2. Install the package using the package manager specific to your system.
- Debian/Ubuntu:
   ```
   sudo apt install /path/to/sff_<VERSION>_amd64.deb
   ```
- Arch Linux:
   ```
   sudo pacman -U /path/to/sff-<VERSION>-x86_64.pkg.tar.zst
   ```
- Fedora:
   ```
   sudo dnf install /path/to/sff-<VERSION>.x86_64.rpm
   ```

#### FreeBSD:

- Install the package from the official repositories:
   ```
   sudo pkg install sff
   ```

### Install from AUR (Arch User Repository)

- Using `yay`:
   ```
   yay -S sff
   ```

- Without AUR helpers:
   ```
   git clone https://aur.archlinux.org/sff.git
   cd sff
   makepkg -si
   ```

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
   By default, this will install under `/usr/local`. You can specify an installation prefix using `PREFIX`, for example:
   ```
   sudo make install PREFIX=/usr
   ```
   will install under `/usr`.

   Note: If you used `PREFIX` during installation, you must specify the same `PREFIX` when uninstalling:
   ```
   sudo make uninstall PREFIX=/usr
   ```


## Usage

Simply run `sff` to start the application from the current directory.

While sff is running:
- Press `?` or `F1` to see the list of key bindings for built-in functions.
- Press `alt`+`/` to see the list of key bindings for extension functions and plugins.
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
