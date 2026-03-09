# Changelog


## Unreleased

### Added

* `-o` option to open files on right arrow or 'l' key ([#35][35])

[35]: https://codeberg.org/sylphenix/sff/issues/35


### Changed

*  Moved version info from `-v` to `-h` and removed `-v` option


## 1.3 <small>(2026-03-06)</small>

### Added

* `SFF_OPENER` environment variable to specify the default file opener
* `SFF_SUDOER` environment variable to specify the utility for sudo mode


### Changed

* Status bar now turns red to indicate 'sudo mode', replacing the previous indicator
* Colors now reference the terminal palette instead of being hardcoded
* Path bar text is now bold instead of underlined
* Forced screen refresh during manual reload
* Number of child processes reduced when executing extension script and plugins
* Key binding updated:
    - 'Select all' -> `Ctrl`+`a`
    - 'Clear selection' -> `[`
    - 'Toggle previous path' -> `-`
    - 'Edit file' -> `Alt`+`e`
* Moved 'Edit file' functionality from core program to extension script
* 'Edit file' now supports opening multiple files at once
* FIFO for extension script is now removed only at program exit
* Set minimum steps to 2 for 'Scroll page' and 'Move quarter page'
* 'Change permissions/owner' recursive option now uses interactive prompt


### Removed

* 'Browse mode' feature and `-b` option
* Key binding for 'Go to root directory'


### Fixed

* Incorrect entry drawing under certain conditions
* Missing parent directory creation before config directory creation
* Incorrect display of 'show hidden' state in options menu in search result tab
* Help page content not updating on window resize
* Incorrect display of symlink targets with multi-byte characters in status bar
* Unexpected jumping to root directory via 'Quick find' in search result tab
* 'Invert select' incorrectly including the current file in auto-select mode
* 'Create new file' and 'Show file status' issues on Alpine Linux
* Text preview issues with the preview plugin on Alpine Linux
* Partial matches occurring during home path replacement in path bar
* Uppercase letters not accepted in confirmation prompts
* "Ignored null byte" warnings while executing extension script in Bash


## 1.2 <small>(2025-09-04)</small>

### Added

* macOS support ([#12][12])

[12]: https://codeberg.org/sylphenix/sff/issues/12


### Changed

* Compiler flags changed from `-Os` to `-O2` with `-fstack-protector-strong` added
* Preview FIFO file now cleaned up on exit
* Input string no longer cleared when type-to-navigate fails to enter a directory
* Symlink target paths no longer folded in status bar
* Installation path for sff-extfunc and plugins moved from `libexec/sff/` to `lib/sff/`


### Fixed

* `-b` option not working when running as superuser
* Paths containing invalid encoding characters not handled properly
* Missing warning when `chdir` fails during tab switching


## 1.1 <small>(2025-06-05)</small>

### Added

* Support for plugins
* New 'Preview' plugin ([#3][3])

[3]: https://codeberg.org/sylphenix/sff/issues/3


### Changed

* sff-extfunc installation path moved from `libexec/` to `libexec/sff/`
* Control sequence sent from sff-extfunc to sff simplified
* 'Search via fzf' now provided as a plugin
* 'Extract/create archives' now provided as a plugin
* Prompt text for extension functions optimized and simplified


### Fixed

* `SIGWINCH` and `SIGTSTP` signal interference during extension script execution
* Errors when handling paths with special characters (e.g. `\`, `&`) during file operations


## 1.0 <small>(2025-04-03)</small>

### Changed

* File size value precision set to one decimal place
* `xstrverscmp` function rewritten for better performance
* Implementation of reading pipe data simplified
* Selections now shared with extension script via pipe instead of regular file
* 'Select range' now selects files in start-to-end order
* Tab switching no longer falls back to home directory on `chdir` failure


### Fixed

* Executable file extension not showing in status bar
* "Illegal instruction" error on Chimera Linux ([#1][1])
* 'Go to root directory' key binding issue
* Pager issues due to `LESS` not unset in extension script ([#2][2])

[1]: https://codeberg.org/sylphenix/sff/issues/1
[2]: https://codeberg.org/sylphenix/sff/issues/2


## 0.9 <small>(2025-03-04)</small>

### Added

* New 'Filter' feature
* New 'Quick find' feature


### Changed

* `histstat` and `histpath` linked lists replaced with arrays
* Config directory check/creation moved to extension function call
* sff-extfunc now gets config directory from arguments instead of environment variables
* Extension script buffer initialization and permission setting performed on demand
* `sff_duplicate` function simplified
* `sff_paste` function optimized for better performance


### Fixed

* Buffers not shared between sudo mode and normal mode
* Errors when checking files starting with `-` during file creation
