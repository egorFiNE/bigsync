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
#include <limits.h>
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

void showVersion(void) {
	printf("bigsync version " VERSION "\n");
}

void showHelp(void) {
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
	fflush(stderr);
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
	int fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd == -1) {
		return errno == EEXIST ? 2 : 0;
	}

	// If we were to pass an exam, we would have to check the return value of close() as well.
	// However the system where open() succeeds and close() does is so much FUBAR that it
	// only makes sense to let it burn.
	close(fd);

	return 1;
}

void syncDestination(FILE *dest) {
	// move data to the kernel
	if (fflush(dest) == EOF) {
		printAndFail("Failed to flush destination file: %s\n", strerror(errno));
	}

	// ask the OS to persist that data to storage
	if (fsync(fileno(dest)) == -1) {
		printAndFail("Failed to sync destination file: %s\n", strerror(errno));
	}

	// Yes, just fflush() is not enough.
}

void updateBlockInFile(char *block, FILE *source, FILE *dest, uint64_t readBytes, int sparseMode,
	char *readingMD4, char *storedMD4, char *zeroBlockMD4) {

	if (fseeko(dest, ftello(source)-readBytes, SEEK_SET) == -1) {
		printAndFail("Failed to seek on file: %s\n", strerror(errno));
	}

	off_t blockOffset = ftello(source) - readBytes;
	int isSourceBlockZero = strcmp(readingMD4, zeroBlockMD4) == 0 ? 1 : 0;

	struct stat destStat;
	if (fstat(fileno(dest), &destStat) == -1) {
		printAndFail("Failed to stat destination file: %s\n", strerror(errno));
	}

	int destinationMayContainData = !S_ISREG(destStat.st_mode) || blockOffset < destStat.st_size;

	int shouldWriteBlock = 0;

	if (sparseMode == SPARSE_MODE_ON) {
		// Seeking would preserve existing bytes, so overwrite them with zeros.
		if (isSourceBlockZero && (storedMD4 || destinationMayContainData)) {
			shouldWriteBlock = 1;

		// Extend the hole before recording its checksum.
		} else if (isSourceBlockZero) {
			if (ftruncate(fileno(dest), blockOffset + readBytes) == -1) {
				printAndFail("Failed to extend sparse file: %s\n", strerror(errno));
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
	}

	syncDestination(dest);
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
	if (fflush(checksumsFile) == EOF) {
		printAndFail("Failed to flush checksum file %s: %s\n", checksumsFilename, strerror(errno));
	}

	off_t checksumsFileTruncation = ftello(checksumsFile);

	if (checksumsFileTruncation == (off_t)-1) {
		printAndFail("Failed to get checksum file position %s: %s\n", checksumsFilename, strerror(errno));
	}

	if (ftruncate(fileno(checksumsFile), checksumsFileTruncation) == -1) {
		printAndFail("Failed to truncate file %s: %s\n", checksumsFilename, strerror(errno));
	}
	if (fclose(checksumsFile) == EOF) {
		printAndFail("Failed to close checksum file %s: %s\n", checksumsFilename, strerror(errno));
	}
}

char *createDestFilenamePath(char *destFilenameArgument, char *sourceFilename) {
	struct stat fileStat;

	// doesn't exists
	if (stat(destFilenameArgument, &fileStat) == -1) {
		// we dup because we have to return a freeable pointer
		return strdup(destFilenameArgument);
	}

	// exists

	char *destFilenameNormalized = realpath(destFilenameArgument, NULL);
	// doesn't exists (?)
	if (!destFilenameNormalized) {
		printAndFail("Cannot open (0) %s: %s\n", destFilenameArgument, strerror(errno));
	}

	if (S_ISDIR(fileStat.st_mode)) {
		char *destFilename;
		asprintf(&destFilename, "%s/%s", destFilenameNormalized, basename(sourceFilename));
		free(destFilenameNormalized);
		return destFilename;
	}

	return destFilenameNormalized;
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
	MD4Update (&mdContext, (unsigned char *)block, size);
	MD4Final (digest, &mdContext);

	char md4[33];
	memset(md4, 0, 33);

	int i;
	for (i = 0; i < 16; i++) {
		sprintf(md4 + i * 2, "%02x", digest[i]);
	}

	strncpy(md4Result, md4, 33);
}

off_t parseBlockSizeArgument(char *optarg) {
	if (*optarg == '\0' || strspn(optarg, "0123456789") != strlen(optarg)) {
		return 0;
	}

	errno = 0;
	unsigned long long megabytes = strtoull(optarg, NULL, 10);

	if (errno || megabytes == 0) {
		return 0;
	}

	off_t bs = (megabytes * 1024ULL * 1024ULL);
	if (bs > UINT_MAX) {
		return 0;
	}

	return bs;
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

	char *destFilenameArgument = NULL;
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
				destFilenameArgument = strdup(optarg);
				break;

			case 'b':
				// strncmp(..., 1) also accepts _garbage as the test-only block size.
				// strcmp accepts only the intended _ token, so malformed arguments are rejected.
				// Here, optarg points to a null-terminated command-line argument,
				// and "_" is a string literal so using strcmp is safe.
				if (strcmp(optarg, "_") == 0) {
					blockSize = 100000;
				} else {
					blockSize = parseBlockSizeArgument(optarg);
					if (blockSize <= 0) {
						printAndFail("Invalid block size: %s\n", optarg);
					}
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

	if (sourceFilename == NULL || destFilenameArgument == NULL) {
		showHelp();
		exit(1);
	}

	if (blockSize <= 0 || (uint64_t) blockSize > UINT_MAX) {
		printAndFail("Invalid block size\n");
	}

	block = malloc((size_t) blockSize);
	if (block == NULL) {
		printAndFail("Cannot allocate block buffer: %s\n", strerror(errno));
	}

	char *destFilename = createDestFilenamePath(destFilenameArgument, sourceFilename);

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

	int destinationWasRecreated = 0;
	if (!shouldOnlyRebuildChecksumsFile) {
		if (fileSize(destFilename) < 0) {
			int createResult = createEmptyFile(destFilename);
			if (!createResult) {
				printAndFail("Cannot create %s: %s\n", destFilename, strerror(errno));
			}
			destinationWasRecreated = createResult == 1;
		}

		destFile = fopen(destFilename, "r+");
		if (destFile == NULL) {
			printAndFail("Cannot open %s: %s\n", destFilename, strerror(errno));
		}

	} else {
		printf("Note: only rebuilding checksum file\n");
	}

	sourceFile = fopen(sourceFilename, "r");
	if (sourceFile == NULL) {
		printAndFail("Cannot open %s: %s\n", sourceFilename, strerror(errno));
	}

	if (checksumsFilename == NULL) {
		asprintf(&checksumsFilename, "%s.bigsync", destFilename);
	}

	if (fileSize(checksumsFilename) < 0) {
		if (!createEmptyFile(checksumsFilename)) {
			printAndFail("Cannot create %s: %s\n", checksumsFilename, strerror(errno));
		}

	} else {
		if (fileSize(checksumsFilename) % 33 > 0) {
			printAndFail("Size of checksums file %s is not dividable by 33, therefore it's broken.\n", checksumsFilename);
		}
	}

	checksumsFile = fopen(checksumsFilename, "r+");
	if (checksumsFile == NULL) {
		printAndFail("Cannot open %s: %s\n", checksumsFilename, strerror(errno));
	}
	if (destinationWasRecreated && ftruncate(fileno(checksumsFile), 0) == -1) {
		printAndFail("Failed to invalidate checksum file %s: %s\n", checksumsFilename, strerror(errno));
	}

	memset(block, 0, blockSize);

	char zeroBlockMD4[33];
	calcMD4(block, (uint64_t) blockSize, zeroBlockMD4);

	while((readBytes = fread(block, 1, blockSize, sourceFile))) {
		checkForErrorAndExit(sourceFile, sourceFilename);
		totalBytesRead += readBytes;

		readingMD4[0] = 0;
		calcMD4(block, (uint64_t) readBytes, readingMD4);

		storedMD4[0] = 0;
		int checksumEntryRead = fgets(storedMD4, 34, checksumsFile) != NULL;
		int isChecksumUsable = checksumEntryRead;
		off_t blockOffset = ftello(sourceFile) - readBytes;
		struct stat currentDestStat;
		if (isChecksumUsable && !shouldOnlyRebuildChecksumsFile) {
			if (fstat(fileno(destFile), &currentDestStat) == -1) {
				printAndFail("Failed to stat destination file: %s\n", strerror(errno));
			}

			if (S_ISREG(currentDestStat.st_mode) &&
				(blockOffset < 0 || blockOffset > currentDestStat.st_size ||
				 (uint64_t) (currentDestStat.st_size - blockOffset) < readBytes)) {
				isChecksumUsable = 0;
			}
		}
		if (isChecksumUsable) {
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

			if (!shouldOnlyRebuildChecksumsFile) {
				updateBlockInFile(block, sourceFile, destFile, readBytes, sparseMode, readingMD4, NULL, zeroBlockMD4);
			}
			updateMD4InChecksumsFile(readingMD4, checksumsFile, checksumEntryRead ? CHECKSUM_REPLACE : CHECKSUM_ADD);

			totalBytesWritten += readBytes;
			totalBlocksChanged++;
		}
	}

	free(block);
	checkForErrorAndExit(sourceFile, sourceFilename);
	showProgressEnd(reportMode);

	off_t lastSourceFileOffset = ftello(sourceFile);

	fclose(sourceFile);

	// Append a single char and cut it off later, so that the file will be of the right size even if the last blocks were sparse
	if (sparseMode == SPARSE_MODE_ON && !shouldOnlyRebuildChecksumsFile) {
		if (reportMode == REPORT_MODE_VERBOSE) {
			printf("Fixing sparse file\n");
		}

		char trailer[1] = "Z";
		if (fseeko(destFile, lastSourceFileOffset, SEEK_SET) == -1 ||
			fwrite(trailer, 1, 1, destFile) != 1) {
			printAndFail("Failed to extend sparse destination: %s\n", strerror(errno));
		}
		syncDestination(destFile);
	}

	if (reportMode == REPORT_MODE_VERBOSE) {
		printf("Truncating file %s to %" PRId64 "\n", destFilename, (uint64_t) lastSourceFileOffset);
	}

	if (!shouldOnlyRebuildChecksumsFile && truncateMode) {
		if (truncate(destFilename, lastSourceFileOffset) < 0) {
			printAndFail("Failed to truncate %s: %s\n", destFilename, strerror(errno));
		}
	}
	if (!shouldOnlyRebuildChecksumsFile && destFile != NULL) {
		syncDestination(destFile);
		if (fclose(destFile) == EOF) {
			printAndFail("Failed to close destination file: %s\n", strerror(errno));
		}
	}
	truncateAndCloseChecksumsFile(checksumsFile, checksumsFilename);

	gettimeofday(&endedAt, &tzp);

	if (reportMode == REPORT_MODE_VERBOSE) {
		showGrandTotal(totalBytesRead, totalBytesWritten, totalBlocksChanged);
		showElapsedTime(endedAt.tv_sec - startedAt.tv_sec);
	}

	free(sourceFilename); // not really needed but makes scan-build happy
	free(destFilename);

	return 0;
}
