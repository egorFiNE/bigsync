# Changelog

## [0.5.0] - 2026-09-22

### Changed

- Changelog converted to standard https://keepachangelog.com/ format
- Makefile -> cmake. It's time, really.
- Removed manpage. The last person on Earth who knew troff formatting is no longer with us.
- Documented historical checksum behavior
- Reject invalid block sizes before opening output files
- Abort on source read errors before truncating backups
- Write zero blocks over existing data in sparse mode (checks whether destination data exists before skipping zero writes)
- Invalidate stale checksums for recreated or shortened backups
- Flush and sync destination data before recording checksums
- Additional error checks added
- Compilation of ancient md4.c on recent compilers fixed
- Avoid writing the sparse trailer with --notruncate
- No breaking changes
- Multitude of tests added

## [0.4.1] - 2020-11-10

### Changed

- Additional validations of source and destination paths, additional error handling in file operations

## [0.4.0] - 2019-03-28

### Added

- Support raw block devices, added --checksum, --notruncate

## [0.3.0] - 2019-03-09

### Added

- added --rebuild

## [0.2.0] - 2018-08-02

### Changed

- zero new features, only minor refactoring;
- updated version number because it is still perfect.

## [0.1.2] - 2013-09-29

### Changed

- moved to github after Google Code killed my repository;
- license changed to MIT.

## [0.1.1] - 2012-11-21

### Fixed

- fixed destination path bug.

## [0.1.0] - 2012-04-14

### Added

- sparse files support, raw devices support, bugfixes, test suite.

## [0.0.9] - 2012-01-09

### Fixed

- fixed critical bugs related to support of large files on 32bit systems; also misc fixes.

## [0.0.8] - 2011-12-10

### Changed

- stat64() changed to stat(); truncate() changed to ftruncate(); misc fixes.

## [0.0.7] - 2011-04-09

### Fixed

- buffer overflow fixed; better compilation options; misc fixes.

## [0.0.6] - 2010-04-13

### Added

- verbose and quiet modes added; misc compilation fixes.

## [0.0.5] - 2009-11-19

### Added

- initial public release.
