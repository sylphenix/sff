# Changelog


## Unreleased

### Changed

* Clean up pipe file on exit
* The status bar turns red to indicate `Sudo mode`, replacing the previous indicator
* Colors now reference the terminal palette instead of being hardcoded
* The current path is now styled with bold instead of underline
* Force screen refresh during manual reload
* Reduced child processes when executing extension scripts and plugins


### Removed

* Removed the `Browse mode` feature and the `-b` option due to lack of use


### Fixed

* Fixed incorrect entry drawing under certain conditions
* Create parent directory before config directory creation
* Correctly display showhidden state in options menu within search result tab
* Fixed help page content not updating on window resize
* Correctly display link targets and extensions with multi-byte characters in status bar


## 1.2 <small>(2025-09-04)</small>

### Added

* Added macOS support ([#12][12])

[12]: https://codeberg.org/sylphenix/sff/issues/12


### Changed

* Changed compiler flags from `-Os` to `-O2` and added `-fstack-protector-strong`
* Clean up preview FIFO file on exit
* When type-to-navigate fails to enter a directory, the input string is no longer cleared
* Symlink target paths no longer folded in status bar
* Changed sff-extfunc and plugins installation path from libexec/sff/ to lib/sff/


### Fixed

* Fixed `-b` option not working when running as superuser
* Properly handle paths containing invalid encoding characters
* Now shows a warning when chdir fails during tab switching


## 1.1 <small>(2025-06-05)</small>

### Added

* Added plugins support
* New preview plugin ([#3][3])

[3]: https://codeberg.org/sylphenix/sff/issues/3


### Changed

* Changed sff-extfunc installation path from libexec/ to libexec/sff/
* Simplified the control sequence sent from sff-extfunc to sff
* 'Search via fzf' now provided as a plugin
* 'Extract and create archives' now provided as a plugin
* Optimized and simplified prompt text for extension functions


### Fixed

* Fixed SIGWINCH/SIGTSTP signal interference during the extension script execution
* Fixed handling of paths with special characters (e.g. `\` `&`) during file operations


## 1.0 <small>(2025-04-03)</small>

### Changed

* Set the precision of the file size value to one decimal place
* Rewrote the `xstrverscmp` function for better performance
* Simplified the implementation of reading pipe data
* Now shares selections with extension script via a pipe instead of a regular file
* Select range feature now selects files in start-to-end order
* Tab switching no longer falls back to home directory on chdir failure


### Fixed

* Fixed issue where the executable file extension was not showing in the status bar
* Fixed 'Illegal instruction' error on Chimera Linux ([#1][1])
* Fixed go to root directory key binding issue
* Unset `LESS` in the extension script to ensure proper pager behavior ([#2][2])

[1]: https://codeberg.org/sylphenix/sff/issues/1
[2]: https://codeberg.org/sylphenix/sff/issues/2


## 0.9 <small>(2025-03-04)</small>

### Added

* New filter feature
* New quick find feature


### Changed

* Replaced `histstat` and `histpath` linked lists with arrays
* Moved config directory check/creation to extension function call
* Centralized memory allocation for `cfgpath`, `extfunc`, `selpath` and `pipepath`
* Extension script now gets config directory from arguments instead of environment variables
* Extension script initializes buffers and sets their permissions when needed
* Simplified extension script `sff_duplicate` function
* Optimized extension script `sff_paste` function for better performance


### Fixed

* Fixed issue where sudo mode and normal mode did not share buffers
* Fixed error when checking files starting with '-' during file creation
