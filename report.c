/*****************************************************************************
*
* Motorola Inc.
* (c) Copyright 2001,2002 Motorola, Inc.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* File Name:         flash_over_jtag.c
*
* Description:       Printing and logging
*
* Modules Included:
*	int print(const char *format, ...);
*	int open_log(char *path);
*	void close_log(void);
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static int log=0;
FILE *log_file;

/* prints messages on stdout and into log file */
int print(const char *format, ...) {
	char *list;
	int i;
	va_start(list, format);
	i=vprintf(format, list);
	if (log) vfprintf(log_file, format, list);
	return(i);
}

/* opens log file */
int open_log(char *path) {
	time_t timer;
	log_file=fopen(path,"wb");
	if (log_file==NULL) {
		printf("Cannot open log file \"%s\"\r\n",path);
		return(-1);
	}
	printf("Log file: %s\r\n",path);
	log=1;
	time(&timer);
	print("Timestamp: %s\r", ctime(&timer));
	return(0);
}

/* closes log file */
void close_log(void) {
	if (log) fclose(log_file);
}
