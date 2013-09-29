# bigsync 

bigsync: backup large files to a slow media.

Bigsync will read the source file in chunks calculating checksums for each one.
It will compare them with previously stored values for the destination file and
overwrite changed chunks in it if checksums differ.

```
Usage: bigsync [options]
  --source <path>     | -s <path>        source file name to be read (mandatory)
  --dest <path>       | -d <path>        destination, file name or directory
                                         (if directory specified, then file will
                                         have the same name in that directory,
                                         mandatory)
  --blocksize <MB>    | -b <MB>          block size in MB, defaults to 15

  --verbose           | -v               verbose output
  --quiet             | -q               only show errors
	--sparse            | -S               destination file to be sparsa (man dd)

  --version           | -V               version number
  --help              | -h               this help
```

Examples:

```
  bigsync --source /home/egor/Documents.dmg --dest /media/backup/documents.dmg.backup
  
  bigsync --blocksize 4 --verbose --source /home/egor/WinSucks.vdi --dest /backup/virtualmachines/
```

See `man bigsync(1)` for more info.

