#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdint.h>
#include <libgen.h>
#include <inttypes.h>
#include "md4_global.h"
#include "md4.h"
#include "hr.h"

#define CHECKSUM_REPLACE 1
#define CHECKSUM_ADD 0

#define PROGRESS_SAME 0
#define PROGRESS_DIFFERENT 1
#define PROGRESS_NOT_EXISTENT 2

#define REPORT_MODE_DEFAULT 0
#define REPORT_MODE_VERBOSE 1
#define REPORT_MODE_QUIET 2

#define SPARSE_MODE_OFF 0
#define SPARSE_MODE_ON 1

#define TRUNCATE_MODE_OFF 0
#define TRUNCATE_MODE_ON 1

#ifndef VERSION
#define VERSION "0.0.0"
#endif

void showVersion() {
	printf("bigsync version " VERSION "\n");
}

void showHelp() {
	showVersion();
	printf(
		"\n" \
		"bigsync: backup large files to a slow media.\n\n" \
		"Bigsync will read the source file in chunks calculating checksums for each one.\n" \
		"It will compare them with previously stored values for the destination file and\n" \
		"overwrite changed chunks in it if checksums differ.\n\n" \
		"Usage: bigsync [options]\n" \
		"  --source <path>     | -s <path>        source file name to be read (mandatory)\n" \
		"  --dest <path>       | -d <path>        destination, file name or directory\n" \
		"                                         (if directory specified, then file will\n" \
		"                                         have the same name in that directory,\n" \
		"                                         mandatory)\n" \
		"  --blocksize <MB>    | -b <MB>          block size in MB, defaults to 15\n" \
		"  --sparse            | -S               destination file to be sparsa (man dd)\n" \
		"  --rebuild           | -r               only create checksums file, do not actually copy data\n" \
		"  --notruncate        | -t               do not truncate the destinatation file\n" \
		"  --checksum <path>   | -c               file name to use as checksum file\n" \
		"                                         (if none is given then \"<DEST>.bigsync\" is used)\n" \
		"\n" \
		"  --verbose           | -v               verbose output\n" \
		"  --quiet             | -q               only show errors\n" \
		"\n" \
		"  --version           | -V               version number\n" \
		"  --help              | -h               this help\n\n" \
		"Examples:\n" \
		"  bigsync --source /home/egor/Documents.dmg --dest /media/backup/documents.dmg.backup\n" \
		"  bigsync --blocksize 4 --verbose --source /home/egor/WinSucks.vdi --dest /backup/virtualmachines/\n\n" \
		"See man bigsync(1) for more info.\n\n"
	);
}

void printAndFail(const char *fmt, ...) {
	va_list ap;
	va_start(ap,fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

void showGrandTotal(uint64_t totalBytesRead, uint64_t totalBytesWritten, uint64_t totalBlocksChanged) {
	char _totalBytesReadHR[100];
	char _totalBytesWrittenHR[100];
	makeHumanReadableSize(_totalBytesReadHR, totalBytesRead);
	makeHumanReadableSize(_totalBytesWrittenHR, totalBytesWritten);

	printf(
		"Total read = %s\nTotal write = %s\nTotal blocks changed = %" PRId64 "\n",
		_totalBytesReadHR,
		_totalBytesWrittenHR,
		totalBlocksChanged
	);
}

char *makeProgressBar(uint64_t currentPosition, uint64_t totalSize) {
	float percent = (float) currentPosition / totalSize * 100;

	int p = percent * 0.5;
	if (p > 50) {
		p = 50;
	}

	char line[51];
	memset(line, ' ', 50);
	memset(line, '=', p);
	line[50] = 0;

	char *progressbar;
	int res = asprintf(&progressbar, " %3d%% [%s] 100%%", (int) percent, line);
	if (res < 0) {
		// if this is the case, you have other things to worry about. meanwhile let's
		// make sure -Wall stays clear.
	}
	return progressbar;
}

void showProgress(
	uint64_t currentPosition,
	uint64_t totalSize,
	char *readingMD4,
	char *storedMD4,
	int status,
	int reportMode) {

	if (reportMode == REPORT_MODE_QUIET) {
		return;
	}

	if (reportMode == REPORT_MODE_VERBOSE) {
		char _currentPosHR[100];
		makeHumanReadableSize(_currentPosHR, currentPosition);
		char _totalSizeHR[100];
		makeHumanReadableSize(_totalSizeHR, totalSize);

		switch(status) {
			case PROGRESS_SAME:
				printf("%s/%s %s -> same\n", _currentPosHR, _totalSizeHR, readingMD4);
				break;

			case PROGRESS_DIFFERENT:
				printf("%s/%s %s -> %s\n", _currentPosHR, _totalSizeHR, readingMD4, storedMD4);
				break;

			case PROGRESS_NOT_EXISTENT:
				printf("%s/%s %s -> added\n", _currentPosHR, _totalSizeHR, readingMD4);
				break;
		}

		return;
	}

	if (totalSize == 0) {
		char _currentPosHR[100];
		makeHumanReadableSize(_currentPosHR, currentPosition);
		printf("\rSyncing %s out of unknown size  ", _currentPosHR);

	} else {
		char *progressBar = makeProgressBar(currentPosition, totalSize);
		printf("\r%s ", progressBar);
		free(progressBar);
	}

	fflush(stdout);
}

void showProgressEnd(int reportMode) {
	if (reportMode == REPORT_MODE_DEFAULT) {
		printf("\n");
		fflush(stdout);
	}
}

void showStartingInfo(char *sourceFilename, char *destFilename, uint64_t sourceSize, uint64_t blockSize) {
	char _blockSizeHR[100];
	char _sourceSizeHR[100];

	makeHumanReadableSize(_blockSizeHR, blockSize);
	makeHumanReadableSize(_sourceSizeHR, sourceSize);

	printf("%s -> %s, %s, block size = %s\n", sourceFilename, destFilename, _sourceSizeHR, _blockSizeHR);
}

void showElapsedTime(uint64_t elapsedTime) {
	char _elapsedTimeHR[200];

	makeHumanReadableTime(_elapsedTimeHR, elapsedTime);

	printf("Elapsed %s\n", _elapsedTimeHR);
}

off_t fileSize(char *filename) {
 	struct stat fileStat;

  if (stat(filename,&fileStat) == -1) {
		return -1;
	}

	return fileStat.st_size;
}

int createEmptyFile(char *filename) {
	FILE *f = fopen(filename, "w");

	if (NULL == f) {
		return 0;
	}

	fclose(f);

	return 1;
}

void updateBlockInFile(char *block, FILE *source, FILE *dest, uint64_t readBytes, int sparseMode,
	char *readingMD4, char *storedMD4, char *zeroBlockMD4) {

	if (fseeko(dest, ftello(source)-readBytes, SEEK_SET) == -1) {
		printAndFail("Failed to seek on file: %s\n", strerror(errno));
	}

	int isSourceBlockZero = strcmp(readingMD4, zeroBlockMD4) == 0 ? 1 : 0;

	int shouldWriteBlock = 0;

	if (sparseMode == SPARSE_MODE_ON) {
		// Source block is zero, but destination block exists
		// and it has a different checksum (because if the checksums
		// were equal - this function wouldn't have called);
		// This means previous data is not zero and must be overwritten explicitly.
		if (isSourceBlockZero && storedMD4) {
			shouldWriteBlock = 1;

		// Source block is zero, but destination block doesn't exists - we can just seek
		// and make it sparse.
		} else if (isSourceBlockZero) {
			// Actually seek here does nothing, but we still do just to check for possible error.
			if (fseeko(dest, readBytes, SEEK_CUR) == -1) {
				printAndFail("Failed to sparse seek in file: %s\n", strerror(errno));
			}
			shouldWriteBlock = 0;

		// Source block is not zero.
		} else {
			shouldWriteBlock = 1;
		}

	// In no sparse mode we have to always overwrite destination block.
	} else {
		shouldWriteBlock = 1;
	}

	if (shouldWriteBlock) {
		if (fwrite(block, 1, readBytes, dest) != readBytes) {
			printAndFail("Failed to write to file: %s\n", strerror(errno));
		}

		if (fsync(fileno(dest)) == -1) {
			printAndFail("Failed to sync destination file: %s\n", strerror(errno));
		}
	}
}

void updateMD4InChecksumsFile(char *readingMD4, FILE *checksumsFile, int doReplaceLastChecksumOrAdd) {
	char writeMD4[34];
	sprintf(writeMD4, "%s\n", readingMD4);

	if (doReplaceLastChecksumOrAdd == CHECKSUM_REPLACE) {
		if (fseeko(checksumsFile, -33, SEEK_CUR) == -1) {
			printAndFail("Failed to seek on file: %s\n", strerror(errno));
		}
	}

	if (fputs(writeMD4, checksumsFile) == EOF) {
		printAndFail("Failed to write to file: %s\n", strerror(errno));
	}
}

void truncateAndCloseChecksumsFile(FILE *checksumsFile, char *checksumsFilename) {
	uint64_t checksumsFileTruncation = ftell(checksumsFile);
	if (ftruncate(fileno(checksumsFile), checksumsFileTruncation) == -1) {
		printAndFail("Failed to truncate file %s: %s\n", checksumsFilename, strerror(errno));
	}
	fclose(checksumsFile);
}

int isDirectory(char *fileOrDirectoryName) {
 	struct stat fileStat;
  if (stat(fileOrDirectoryName, &fileStat) != 0) {
		return -1;
	}

	if (fileStat.st_mode & S_IFBLK) {
		return 0;
	} else if (fileStat.st_mode & S_IFDIR) {
		return 1;
	} else {
		return 0;
	}
}

char *createDestFilenameFromSource(char *directoryName,  char *sourceFilename) {
	char *_sourceFilename = basename(sourceFilename);

	directoryName = realloc(directoryName, strlen(directoryName) + strlen(_sourceFilename) + 1);

	if (directoryName[strlen(directoryName)-1] == '/') {
		directoryName[strlen(directoryName)-1] = 0;
	}

	sprintf(directoryName, "%s/%s", directoryName, _sourceFilename);

	return directoryName;
}

void checkForErrorAndExit(FILE *file, char *fileName) {
	if (ferror(file)) {
		printAndFail("Cannot read %s at %" PRId64 ": %s\n", fileName, ftello(file), strerror(errno));
	}
}

void calcMD4(char *block, uint64_t size, char *md4Result) {
	unsigned char digest[16];
	MD4_CTX mdContext;
	MD4Init (&mdContext);
	MD4Update (&mdContext, block, size);
	MD4Final (digest, &mdContext);

	char md4[33];
	bzero(md4, 33);

	int i;
	for (i = 0; i < 16; i++) {
		sprintf(md4, "%s%02x", md4, digest[i]);
	}

	strncpy(md4Result, md4, 33);
}

int main(int argc, char *argv[]) {
	int reportMode = REPORT_MODE_DEFAULT;
	int sparseMode = SPARSE_MODE_OFF;
	int truncateMode = TRUNCATE_MODE_ON;
	int shouldOnlyRebuildChecksumsFile = 0;

	int shouldAssumeZeroSourceSize = 0;

 	off_t sourceSize = 0;
	char *sourceFilename = NULL;
	FILE *sourceFile = NULL;

	char *destFilename = NULL;
	FILE *destFile = NULL;

	char *checksumsFilename = NULL;
	FILE *checksumsFile = NULL;

	char *block = NULL;
	uint64_t readBytes = 0;
	off_t totalBytesRead = 0;

	off_t totalBytesWritten = 0;
	off_t totalBlocksChanged = 0;

	off_t blockSize = 1024 * 1024 * 15;

	char readingMD4[34];
	char storedMD4[34];

	char sourceSizeHR[100];

	struct timeval startedAt;
	struct timeval endedAt;
	struct timezone tzp;

	int ch;

	/* options descriptor */
	static struct option longopts[] = {
		{ "source",    required_argument, NULL,       's' },
		{ "dest",      required_argument, NULL,       'd' },
		{ "blocksize", required_argument, NULL,       'b' },
		{ "sparse",    no_argument,       NULL,       'S' },
		{ "help",      no_argument,       NULL,       'h' },
		{ "verbose",   no_argument,       NULL,       'v' },
		{ "quiet",     no_argument,       NULL,       'q' },
		{ "version",   no_argument,       NULL,       'V' },
		{ "rebuild",   no_argument,       NULL,       'r' },
		{ "notruncate", no_argument,      NULL,       't' },
		{ "checksum",  required_argument, NULL,       'c' },
		{ "zero",      no_argument,       NULL,       '@' }, // test-only mode
		{ NULL,        0,                 NULL,       0   }
	};

	while ((ch = getopt_long(argc, argv, "s:d:b:hvqStc:@", longopts, NULL)) != -1) {
		switch (ch) {
			case '@':
				shouldAssumeZeroSourceSize = 1;
				break;

			case 's':
				sourceFilename = strdup(optarg);
				break;

			case 'd':
				destFilename = strdup(optarg);
				break;

			case 'b':
				if (strncmp(optarg, "_", 1) == 0) {
					blockSize = 100000;
				} else {
					blockSize = atoi(optarg);
					blockSize = blockSize * 1024 * 1024;
				}
				break;

			case 'v':
				reportMode = REPORT_MODE_VERBOSE;
				break;

			case 'S':
				sparseMode = SPARSE_MODE_ON;
				break;

			case 'q':
				reportMode = REPORT_MODE_QUIET;
				break;

			case 'r':
				shouldOnlyRebuildChecksumsFile = 1;
				break;

			case 't':
				truncateMode = TRUNCATE_MODE_OFF;
				break;

			case 'c':
				checksumsFilename = strdup(optarg);
				break;

			case 'V':
				showVersion();
				exit(1);
				break;

			case 'h':
			default:
				showHelp();
				exit(1);
				break;
		}
	}

	if (sourceFilename == NULL || destFilename == NULL) {
		showHelp();
		exit(1);
	}

	if (isDirectory(destFilename) == 1) {
		destFilename = createDestFilenameFromSource(destFilename, sourceFilename);
	}

	sourceSize = fileSize(sourceFilename);
	if (shouldAssumeZeroSourceSize) {
		sourceSize = 0;
	}

	if (sourceSize < 0) {
		printAndFail("File %s does not exists or could not be read\n", sourceFilename);
	}

	makeHumanReadableSize(sourceSizeHR, sourceSize);

	gettimeofday(&startedAt, &tzp);

	if (reportMode == REPORT_MODE_VERBOSE) {
		showStartingInfo(sourceFilename, destFilename, sourceSize, blockSize);
	}

	if (!shouldOnlyRebuildChecksumsFile) {
		if (fileSize(destFilename) < 0) {
			if (!createEmptyFile(destFilename)) {
				printAndFail("Cannot create %s: %s\n", destFilename, strerror(errno));
			}
		}

		destFile = fopen(destFilename, "r+");
	} else {
		printf("Note: only rebuilding checksum file\n");
	}

	sourceFile = fopen(sourceFilename, "r");

	if (checksumsFilename == NULL) {
		checksumsFilename = malloc(strlen(destFilename) + 9);
		sprintf(checksumsFilename, "%s.bigsync", destFilename);
	}

	if (fileSize(checksumsFilename) < 0) {
		if (!createEmptyFile(checksumsFilename)) {
			printAndFail("Cannot create %s: %s\n", checksumsFilename, strerror(errno));
		}

	} else {
		if (fileSize(checksumsFilename) % 33 > 0) {
			printAndFail("Size of checksums file %s is not dividable by 33, therefore it's broken.\n",
				checksumsFilename);
		}
	}

	checksumsFile = fopen(checksumsFilename, "r+");
	if (checksumsFile == NULL) {
		printAndFail("Cannot open %s: %s\n", checksumsFilename, strerror(errno));
	}

	block = malloc(blockSize);
	bzero(block, blockSize);

	char zeroBlockMD4[33];
	calcMD4(block, (uint64_t) blockSize, zeroBlockMD4);

	while((readBytes = fread(block, 1, blockSize, sourceFile))) {
		checkForErrorAndExit(sourceFile, sourceFilename);
		totalBytesRead += readBytes;

		readingMD4[0] = 0;
		calcMD4(block, (uint64_t) readBytes, readingMD4);

		storedMD4[0] = 0;
		if (fgets(storedMD4, 34, checksumsFile)) {
			storedMD4[32] = 0;

			if (strncmp(storedMD4, readingMD4, 32) == 0) {
				showProgress((uint64_t) ftello(sourceFile), sourceSize, readingMD4, storedMD4, PROGRESS_SAME, reportMode);

			} else {
				showProgress((uint64_t) ftello(sourceFile), sourceSize, readingMD4, storedMD4, PROGRESS_DIFFERENT, reportMode);

				if (!shouldOnlyRebuildChecksumsFile) {
					updateBlockInFile(block, sourceFile, destFile, readBytes, sparseMode, readingMD4, storedMD4, zeroBlockMD4);
				}
				updateMD4InChecksumsFile(readingMD4, checksumsFile, CHECKSUM_REPLACE);

				totalBytesWritten += readBytes;
				totalBlocksChanged++;
			}

		} else {
			checkForErrorAndExit(checksumsFile, checksumsFilename);
			showProgress(ftello(sourceFile), sourceSize, readingMD4, NULL, PROGRESS_NOT_EXISTENT, reportMode);

			updateMD4InChecksumsFile(readingMD4, checksumsFile, CHECKSUM_ADD);

			if (!shouldOnlyRebuildChecksumsFile) {
				updateBlockInFile(block, sourceFile, destFile, readBytes, sparseMode, readingMD4, NULL, zeroBlockMD4);
			}

			totalBytesWritten += readBytes;
			totalBlocksChanged++;
		}
	}

	free(block);
	showProgressEnd(reportMode);

	off_t lastSourceFileOffset = ftello(sourceFile);

	fclose(sourceFile);

	truncateAndCloseChecksumsFile(checksumsFile, checksumsFilename);

	// Append a single char and cut it off later, so that the file will be of the right size even if the last blocks were sparse
	if (sparseMode == SPARSE_MODE_ON && !shouldOnlyRebuildChecksumsFile) {
 	 if (reportMode == REPORT_MODE_VERBOSE) {
			printf("Fixing sparse file\n");
		}

		char trailer[1] = "Z";
		fseeko(destFile, lastSourceFileOffset, SEEK_SET);
		fwrite(trailer, 1, 1, destFile);
		fclose(destFile);
	}

	if (reportMode == REPORT_MODE_VERBOSE) {
		printf("Truncating file %s to %" PRId64 "\n", destFilename, (uint64_t) lastSourceFileOffset);
	}

	if (!shouldOnlyRebuildChecksumsFile && truncateMode) {
		if (truncate(destFilename, lastSourceFileOffset) < 0) {
			printAndFail("Failed to truncate %s: %s\n", destFilename, strerror(errno));
		}
	}

	gettimeofday(&endedAt, &tzp);

	if (reportMode == REPORT_MODE_VERBOSE) {
		showGrandTotal(totalBytesRead, totalBytesWritten, totalBlocksChanged);
		showElapsedTime(endedAt.tv_sec - startedAt.tv_sec);
	}

	free(sourceFilename); // not really needed but makes scan-build happy

	return 0;
}
