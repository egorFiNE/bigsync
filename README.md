# Note: bigsync is rarely updated, because it is nearly perfect

# bigsync 

Bigsync is a tool to incrementally backup a single large file to a slow destination (think network media or a cheap NAS). The most common cases for bigsync are disk images, virtual OS images, encrypted volumes and raw devices.

Bigsync will read the source file in chunks calculating checksums for each one. It will compare them with previously stored values for the destination file and overwrite changed chunks if checksums differ.

This way we minimize the access to a slow target media which is the whole point of bigsync's existence.

## Usage

Run this periodically: 

```
bigsync --source /home/egor/Documents.dmg --dest /media/backup/documents.dmg.backup
```

For more details see `man bigsync`. 

## bigsync vs rsync

rsync does kind of the same thing, too. But rsync does read both files to calculate checksums, which slows down the whole process a lot when working with slow media. bigsync only reads source file and writes only the changed blocks to destination, which minimizes load and access to the destination drive.

## Installation

Download source. make. make install. make test. 

## Supported OS

"Officially" used in and compatible with:

* Any Linux in both 32bit and 64bit;
* macOS (aka OS X aka Mac OS X): 
  * any modern version for ARM
  * 10.5+ both PPC and Intel in both 32bit and 64bit.

Perhaps bigsync will work on any POSIX-compatible unix except really ancient ones.

## Bugreports/suggestions/patches

Open an issue ticket at GitHub: https://github.com/egorFiNE/bigsync/issues

## History

See `Changelog`. Note: bigsync is rarely updated, because it is nearly perfect. 

## Acknowledgments

Special thanks to Western Digital for producing «MyBook World Edition», which is so incredibly slow that it motivated me to create bigsync.

Thanks to Andrew Suslov for helping me. Thanks to Dirk Huffer for sparse files support.
