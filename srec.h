/*****************************************************************************
*
* Motorola Inc.
* (c) Copyright 2001,2002 Motorola, Inc.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* File Name:         flash.h
*
* Description:       Prototypes for S-record file proccessing
*
* Modules Included:  None
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
****************************************************************************/

#ifndef SREC_H__
#define SREC_H__

#include "flash.h"
#include "flash_over_jtag.h"

#define MAX_LINE_LENGTH		300					/* max line length in input S files */
#define MAX_WORDS_PER_LINE	MAX_LINE_LENGTH/4	/* max number of data words per s-record file line */
#define OUTPUT_S_REC_DATA_PER_LINE 16

unsigned int hex2dec(char *bcd);
int s_line_process(char *line, unsigned long int *addr, unsigned int *data);
int find_flash(unsigned long int addr, flash_constants flash_param[], int flash_count);
int place_data(unsigned long int addr, unsigned int data, flash_constants flash_param[], int flash_count);
int read_s_record(char *path, flash_constants flash_param[], int flash_count, char *serror);
int write_s_record(char *path, mem_read_constants mem_read);
void s_line_write(FILE *output, int type, int length, long int addr, void *data);
void srec_check_checksums(char i);

#endif
