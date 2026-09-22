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
#include "md4_global.h"
#include "md4.h"


int allTestsPassed=1;

void printAndFail(char *line) {
	printf("%s: %s\n", line, strerror(errno));
	exit(1);
}

void createZeroFile(char *filename, size_t size) {
	FILE *f = fopen(filename, "w");
	if (!f) {
		printAndFail("Cannot create file");
	}

	char *block = (char*) malloc(size);
	memset(block, 0, size);
	if (fwrite(block, 1, size, f) != size) {
		printAndFail("Cannot write data 1");
	}
	fclose(f);
}

void createFile(char *filename, const char *contents) {
	FILE *f = fopen(filename, "w");
	if (!f) {
		printAndFail("Cannot create file");
	}

	size_t size = strlen(contents);
	if (fwrite(contents, 1, size, f) != size) {
		printAndFail("Cannot write data");
	}
	fclose(f);
}

void changeByte(char *filename, off_t position, char byte) {
	FILE *f = fopen(filename, "r+");
	if (!f) {
		printAndFail("Cannot create file");
	}

	if (fseeko(f, position, SEEK_SET)<0) {
		printAndFail("Cannot seek");
	}

	char bytes[1];
	bytes[0]=byte;

	if (fwrite(bytes, 1, 1, f)!=1) {
		printAndFail("Cannot write data 2");
	}

	fclose(f);
}

void addBytes(char *filename, int countOfBytes, char byte) {
	FILE *f = fopen(filename, "a+");
	if (!f) {
		printAndFail("Cannot create file");
	}

	char *bytes= (char*) malloc(countOfBytes+1);
	int i;
	for(i=0;i<countOfBytes;i++) {
		bytes[i]=byte;
	}

	if (fwrite(bytes, 1, countOfBytes, f)!=countOfBytes) {
		printAndFail("Cannot write data 3");
	}

	fclose(f);
	free(bytes);
}


uint32_t blocksCount(char *filename) {
 	struct stat fileStat;
  if (stat(filename,&fileStat)==-1) {
		return -1;
	}
	return (uint32_t) fileStat.st_blocks;
}

off_t fileSize(char *filename) {
	struct stat fileStat;
	if (stat(filename,&fileStat)==-1) {
		return -1;
	}
	return  fileStat.st_size;
}

int checkFileSize(char *testName, char *filename, size_t referenceSize) {
	int64_t sourceFileSize = (int64_t) fileSize(filename);
	if (sourceFileSize == (int64_t) referenceSize) {
		printf("%s (size): Pass\n", testName);
		return 1;
	} else {
		allTestsPassed=0;
		printf("%s (size): FAIL.  Source file size %llu;  Dest file size %llu\n", testName, sourceFileSize, (int64_t) referenceSize);
		return 0;
	}
}

void calcMD4(char *filename, char *md4Result) {
	off_t size = fileSize(filename);

	char *block = (char*) malloc(size);
	memset(block, 0, size);

	FILE *f = fopen(filename, "r");
	fread(block, 1, size, f);
	fclose(f);

	unsigned char digest[16];
	MD4_CTX mdContext;
	MD4Init (&mdContext);
	MD4Update (&mdContext, (unsigned char *)block, size);
	MD4Final (digest, &mdContext);

	char readingMD4[33];
	memset(readingMD4, 0, 33);
	int i;
	for (i = 0; i < 16; i++) {
		sprintf(readingMD4 + i * 2, "%02x", digest[i]);
	}
	strncpy(md4Result, readingMD4, 33);
}

void testHistoricalChecksum(void) {
	const char *message = "abc";
	/* Historical checksums depend on the native unsigned long width. */
	const char *expected = sizeof(UINT4) == 8
		? "a013c51eca433daca5b2a594d21af1fe"
		: "a448017aaf21d8525fc10ae87aa6729d";
	unsigned char digest[16];
	char actual[33];
	MD4_CTX context;

	MD4Init(&context);
	MD4Update(&context, (unsigned char *)message, strlen(message));
	MD4Final(digest, &context);
	for (int i = 0; i < 16; i++) {
		sprintf(actual + i * 2, "%02x", digest[i]);
	}

	if (strcmp(actual, expected) == 0) {
		printf("Historical checksum: Pass\n");
	} else {
		allTestsPassed = 0;
		printf("Historical checksum: FAIL. Expected %s; got %s\n", expected, actual);
	}
}

int syncAndCheckMd4(char *testName, char *sourceFilename, char *destFilename, int isSparse, int isSourceZero) {
	char md4Source[33];
	calcMD4(sourceFilename, md4Source);

	char command[100];
	sprintf(command, "./bigsync --source %s --dest %s --blocksize _ --quiet %s %s",
		sourceFilename, destFilename,
		isSparse ? "--sparse" : "",
		isSourceZero ? "--zero" : ""
	);

	system(command);

	char md4Dest[33];
	calcMD4(destFilename, md4Dest);

	if (strcmp(md4Source, md4Dest)==0) {
		printf("%s (sync): Pass\n", testName);
		return 1;
	} else {
		allTestsPassed=0;
		printf("%s (sync): FAIL.  Source MD4 %s;  Dest MD4 %s\n", testName, md4Source, md4Dest);
		return 0;
	}
}

void cleanup(void) {
	remove("testSource.bin");
	remove("testDest.bin");
	remove("testDest.bin.bigsync");
}

void testSparse(void) {
	cleanup();

	uint32_t sparseFailCount=0;

	createZeroFile("testSource.bin", 1024*1024*5);
	uint32_t sourceBlocksCount = blocksCount("testSource.bin");

	syncAndCheckMd4("recreated source to zero", "testSource.bin", "testDest.bin", 1, 0);
	checkFileSize("recreated source to zero size", "testDest.bin", 1024*1024*5);

	uint32_t destBlocksCount = blocksCount("testDest.bin");
	if (destBlocksCount >= sourceBlocksCount) {
		sparseFailCount++;
	}

	changeByte("testSource.bin", 5, 'c');
	syncAndCheckMd4("changed byte (SPARSE) ", "testSource.bin", "testDest.bin", 1, 0);
	checkFileSize("changed byte (SPARSE) (size)", "testDest.bin", 1024*1024*5);

	destBlocksCount = blocksCount("testDest.bin");
	if (destBlocksCount >= sourceBlocksCount) {
		sparseFailCount++;
	}

	changeByte("testSource.bin", 5, 0);
	syncAndCheckMd4("changed byte to zero (SPARSE) ", "testSource.bin", "testDest.bin", 1, 0);
	checkFileSize("changed byte to zero (SPARSE) (size)", "testDest.bin", 1024*1024*5);

	destBlocksCount = blocksCount("testDest.bin");
	if (destBlocksCount >= sourceBlocksCount) {
		sparseFailCount++;
	}

	addBytes("testSource.bin", 5*1024*1024, 0);
	sourceBlocksCount = blocksCount("testSource.bin");

	syncAndCheckMd4("added 5 md (SPARSE) ", "testSource.bin", "testDest.bin", 1, 0);
	checkFileSize("added 5 md (SPARSE) (size)", "testDest.bin", 1024*1024*10);

	destBlocksCount = blocksCount("testDest.bin");
	if (destBlocksCount >= sourceBlocksCount) {
		sparseFailCount++;
	}

	changeByte("testSource.bin", 7*1024*1024, 'c');
	syncAndCheckMd4("changed one byte (SPARSE) ", "testSource.bin", "testDest.bin", 1, 0);
	checkFileSize("changed one byte (SPARSE) (size)", "testDest.bin", 1024*1024*10);

	destBlocksCount = blocksCount("testDest.bin");
	if (destBlocksCount >= sourceBlocksCount) {
		sparseFailCount++;
	}

	if (sparseFailCount>0) {
		printf("\n  Warning: sparse mode was enabled, but the destination file doesn't seem\n"
			"  to occupy less disk space then the original file. For certain file systems it's\n"
			"  okay as they don't support sparse files.\n\n"
			"  This warning is safe, sparse files support is NOT required for data integrity.\n\n"
		);
	}
}

void testZeroSizedSource(int isSparse) {
	cleanup();

	createZeroFile("testSource.bin", 1024*1024*5);
	syncAndCheckMd4("zero sized source", "testSource.bin", "testDest.bin", isSparse, 1);
	checkFileSize("zero sized source", "testDest.bin", 1024*1024*5);

	changeByte("testSource.bin", 5, 'c');
	syncAndCheckMd4("changed byte (zero sized source) ", "testSource.bin", "testDest.bin", isSparse, 1);
	checkFileSize("changed byte (zero sized source) (size)", "testDest.bin", 1024*1024*5);

	addBytes("testSource.bin", 1*1024*1024, 0);
	syncAndCheckMd4("added 1 md (zero sized source) ", "testSource.bin", "testDest.bin", isSparse, 1);
	checkFileSize("added 1 md (zero sized source) (size)", "testDest.bin", 1024*1024*6);

	truncate("testSource.bin", 2*1024*1024);
	syncAndCheckMd4("truncated (zero sized source) ", "testSource.bin", "testDest.bin", isSparse, 1);
	checkFileSize("truncated (zero sized source) (size)", "testDest.bin", 1024*1024*2);
}

void testCycle(int sparseEnabled) {
	cleanup();

	createZeroFile("testSource.bin", 110000);
	syncAndCheckMd4("sync zero 1", "testSource.bin", "testDest.bin", sparseEnabled, 0);
	checkFileSize("sync zero 1", "testSource.bin", 110000);

	changeByte("testSource.bin", 5, 'c');
	syncAndCheckMd4("one byte changed 1", "testSource.bin", "testDest.bin", sparseEnabled, 0);
	checkFileSize("one byte changed 2", "testDest.bin", 110000);

	changeByte("testSource.bin", 80000, 'c');
	syncAndCheckMd4("one byte changed 1", "testSource.bin", "testDest.bin", sparseEnabled, 0);
	checkFileSize("one byte changed 2", "testDest.bin", 110000);

	addBytes("testSource.bin", 3, 'c');
	syncAndCheckMd4("added 3 bytes 1", "testSource.bin", "testDest.bin", sparseEnabled, 0);
	checkFileSize("added 3 bytes 2", "testDest.bin", 110003);

	addBytes("testSource.bin", 100001, 'c');
	syncAndCheckMd4("added 8 bytes 1", "testSource.bin", "testDest.bin", sparseEnabled, 0);
	checkFileSize("added 8 bytes 2", "testDest.bin", 210004);

	truncate("testSource.bin", 123000);
	syncAndCheckMd4("truncated", "testSource.bin", "testDest.bin", sparseEnabled, 0);
	checkFileSize("truncated", "testDest.bin", 123000);
}

void testBasic(void) {
	cleanup();

	createZeroFile("testSource.bin", 4000);
	changeByte("testSource.bin", 5, 'r');
	syncAndCheckMd4("sync single block", "testSource.bin", "testDest.bin", 0, 0);
	checkFileSize("sync single block size", "testSource.bin", 4000);
}

void testMaintenance(void) {
	char sourceMD4[33];

	cleanup();
	createZeroFile("testSource.bin", 100000);
	changeByte("testSource.bin", 10, 'x');
	syncAndCheckMd4("maintenance setup", "testSource.bin", "testDest.bin", 0, 0);
	calcMD4("testDest.bin", sourceMD4);
	if (system("./bigsync --source testSource.bin --dest testDest.bin --blocksize 0 --quiet") == 0) {
		allTestsPassed = 0;
		printf("invalid block size: FAIL\n");
	} else {
		char destinationMD4[33];
		calcMD4("testDest.bin", destinationMD4);
		if (strcmp(sourceMD4, destinationMD4) != 0) {
			allTestsPassed = 0;
			printf("invalid block size preserves destination: FAIL\n");
		} else {
		printf("invalid block size preserves destination: Pass\n");
		}
	}

	if (system("./bigsync --source . --dest testDest.bin --blocksize _ --quiet") == 0) {
		allTestsPassed = 0;
		printf("source read error: FAIL\n");
	} else {
		char destinationMD4[33];
		calcMD4("testDest.bin", destinationMD4);
		if (strcmp(sourceMD4, destinationMD4) != 0) {
			allTestsPassed = 0;
			printf("source read error preserves destination: FAIL\n");
		} else {
			printf("source read error preserves destination: Pass\n");
		}
	}

	remove("testDest.bin.bigsync");
	createZeroFile("testSource.bin", 100000);
	calcMD4("testSource.bin", sourceMD4);
	createZeroFile("testDest.bin", 100000);
	changeByte("testDest.bin", 10, 'x');
	if (system("./bigsync --source testSource.bin --dest testDest.bin --blocksize _ --sparse --quiet") != 0) {
		allTestsPassed = 0;
		printf("uncached sparse block: FAIL\n");
	} else {
		char destinationMD4[33];
		calcMD4("testDest.bin", destinationMD4);
		if (strcmp(sourceMD4, destinationMD4) != 0) {
			allTestsPassed = 0;
			printf("uncached sparse block: FAIL\n");
		} else {
			printf("uncached sparse block: Pass\n");
		}
	}

	changeByte("testSource.bin", 75000, 'x');
	syncAndCheckMd4("cache before recreation", "testSource.bin", "testDest.bin", 0, 0);
	remove("testDest.bin");
	syncAndCheckMd4("recreated destination", "testSource.bin", "testDest.bin", 0, 0);
	truncate("testDest.bin", 50000);
	syncAndCheckMd4("shortened destination", "testSource.bin", "testDest.bin", 0, 0);
}

void testSparseNoTruncate(void) {
	cleanup();
	createFile("testSource.bin", "hello");
	createFile("testDest.bin", "abcdefghij");

	if (system("./bigsync --source testSource.bin --dest testDest.bin --blocksize _ --sparse --notruncate --quiet") != 0) {
		allTestsPassed = 0;
		printf("sparse no truncate: FAIL\n");
		return;
	}

	FILE *f = fopen("testDest.bin", "r");
	char contents[11] = { 0 };
	if (!f || fread(contents, 1, 10, f) != 10 || strcmp(contents, "hellofghij") != 0 || fileSize("testDest.bin") != 10) {
		allTestsPassed = 0;
		printf("sparse no truncate preserves tail: FAIL\n");
	} else {
		printf("sparse no truncate preserves tail: Pass\n");
	}
	if (f) {
		fclose(f);
	}
}

int main(void) {
	testHistoricalChecksum();
	testBasic();
	testCycle(0);
	testCycle(1);
	testSparse();
	testZeroSizedSource(0);
	testZeroSizedSource(1);
	testMaintenance();
	testSparseNoTruncate();
	cleanup();
	if (allTestsPassed) {
		printf("\nAll tests passed.\n");
	} else {
		printf("\nSome tests failed to pass.\n");
	}
	return 0;
}
