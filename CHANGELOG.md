# Changelog

* [Unreleased](#unreleased)
* [0.9](#0.9)


## Unreleased

### Added
### Changed

* Set the precision of the file size value to one decimal place.
* Rewrote the `xstrverscmp` function for better performance.
* Simplified the implementation of reading pipe data.


### Deprecated
### Removed
### Fixed

Fixed issue where the executable file extension was not showing in the status bar.


### Security
### Contributors


## 0.9

### Added

* Added filter feature.
* Added quick find feature.


### Changed

* Replaced `histstat` and `histpath` linked lists with arrays.
* Moved config directory check/creation to extension function call.
* Centralized memory allocation for `cfgpath`, `extfunc`, `selpath` and `pipepath`.
* Extension script now gets config directory from arguments instead of environment variables.
* Extension script initializes buffers and sets their permissions when needed.
* Simplified extension script `sff_duplicate` function.
* Optimized extension script `sff_paste` function for better performance.


### Fixed

* Fixed issue where sudo mode and normal mode did not share buffers.
* Fixed error when checking files starting with '-' during file creation.
