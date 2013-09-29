#include <stdio.h>
#include <stdint.h>
#include "hr.h"

#define GIGABYTE (1024*1024*1024)
#define MEGABYTE (1024*1024)
#define KILOBYTE 1024

#define MINUTE 60
#define HOUR (60*60)

char *makeHumanReadableSize(char *result, uint64_t size) {
	if (size >= GIGABYTE) { 
		sprintf(result, "%.3f Gb", (float) ((float)size/(float)GIGABYTE));
	} else if (size >= MEGABYTE) { 
		sprintf(result, "%d Mb", (int) (size/MEGABYTE));
	} else if (size >= KILOBYTE) { 
		sprintf(result, "%d Kb", (int) (size/KILOBYTE));
	} else {
		sprintf(result, "%d b", (int) size);
	}
	return result;
}

char *makeHumanReadableTime(char *result, long int elapsedSeconds) {
	int seconds = elapsedSeconds;
	int hours = seconds/HOUR;
	seconds-=hours*HOUR;
	int minutes = seconds/MINUTE;
	seconds-=minutes*MINUTE;

	if (elapsedSeconds >= HOUR) { 
		if (seconds == 0) {
			sprintf(result, "%dh%02dm", hours, minutes);
		} else { 
			sprintf(result, "%dh%02dm%02ds", hours, minutes, seconds);
		}
	} else if (elapsedSeconds >= MINUTE) { 
		if (seconds == 0) {
			sprintf(result, "%dm", minutes);
		} else { 
			sprintf(result, "%dm%02ds", minutes, seconds);
		}
	} else {
		sprintf(result, "%lds", elapsedSeconds);
	}
	return result;
}

/*
int main(void) { 
	char hrTime[1024];
	makeHumanReadableTime(hrTime, 3);
	printf("%s\n", hrTime);
	makeHumanReadableTime(hrTime, 33);
	printf("%s\n", hrTime);
	makeHumanReadableTime(hrTime, 61);
	printf("%s\n", hrTime);
	makeHumanReadableTime(hrTime, 60);
	printf("%s\n", hrTime);
	makeHumanReadableTime(hrTime, 65);
	printf("%s\n", hrTime);
	makeHumanReadableTime(hrTime, 183);
	printf("%s\n", hrTime);
	makeHumanReadableTime(hrTime, 60*60);
	printf("%s\n", hrTime);
	makeHumanReadableTime(hrTime, 60*60+59);
	printf("%s\n", hrTime);
	makeHumanReadableTime(hrTime, 60*64+4);
	printf("%s\n", hrTime);
	makeHumanReadableTime(hrTime, 60*60*4);
	printf("%s\n", hrTime);
	makeHumanReadableTime(hrTime, 60*60*4+4);
	printf("%s\n", hrTime);
}
*/

/*
int main(void) { 
	char hrSize[1024];
	makeHumanReadableSize(hrSize, 5468867682);
	printf("%s\n", hrSize);
	makeHumanReadableSize(hrSize, 546886762);
	printf("%s\n", hrSize);
	makeHumanReadableSize(hrSize, 54688662);
	printf("%s\n", hrSize);
	makeHumanReadableSize(hrSize, 5468862);
	printf("%s\n", hrSize);
	makeHumanReadableSize(hrSize, 546862);
	printf("%s\n", hrSize);
	makeHumanReadableSize(hrSize, 54662);
	printf("%s\n", hrSize);
	makeHumanReadableSize(hrSize, 5462);
	printf("%s\n", hrSize);
	makeHumanReadableSize(hrSize, 562);
	printf("%s\n", hrSize);
	makeHumanReadableSize(hrSize, 52);
	printf("%s\n", hrSize);
	makeHumanReadableSize(hrSize, 5);
	printf("%s\n", hrSize);
	makeHumanReadableSize(hrSize, 0);
	printf("%s\n", hrSize);
}
*/
