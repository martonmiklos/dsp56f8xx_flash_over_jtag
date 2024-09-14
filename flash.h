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
* Description:       Prototype of flash descriptor and config file related functions
*
* Modules Included:  None
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
****************************************************************************/

#ifndef FLASH______H
#define FLASH______H

#define MAX_FLASH_UNITS		32	/* how many flash blocks do we have at maximum */
#define MAX_PAGE_COUNT		128	/* maximum flash size is 32k (128 pages, 256 words each) */
#define MAX_LINE_LENGTH		300	/* max line length in input config file */

typedef struct {
	unsigned int	flash_start;	/* beginning of the block in memory map */
	unsigned int	flash_end;		/* end of block in memory map */
	unsigned int	program_memory;	/* 1-pflash, 0-dflash */
	unsigned int	interface_address; /* address of the interface block in memmory */
	unsigned int	terasel;		/* timing constants */
	unsigned int	tmel;
	unsigned int	tnvsl;
	unsigned int	tpgsl;
	unsigned int	tprogl;
	unsigned int	tnvhl;
	unsigned int	tnvhl1;
	unsigned int	trcvl;
	unsigned int	clk_divisor;
	unsigned int	start_addr;		/* start address of data other than 0xffff */
	unsigned int	data_count;		/* length of data other than 0xffff */
	unsigned int	*data;			/* pointer to array where data are stored */
	unsigned int	duplicate;		/* 0 - first occurence, 1 - next occurence of the same interface address */
	unsigned int 	*page_erase_map; /* bit0: 0=not yet erased, 1=already erased */
									 /* bit1: 1=request to erase this page (set by S-rec processing routine), 0=preserve page */
} flash_constants;

/* Comments:

start_addr and data_count will assure that 0xffffs at beginning and end of the block will not be programmed (saves time)
duplicate will assure that each flash is mass erased only once

for duplicate pages the page_erase_map is not freshly allocated, pointer to the first map is used instead. This assures that only one map is allocated per flash block

*/

int read_setup(char *path, flash_constants flash_param[]);
int flash_prepare(flash_constants flash_param[], int flash_count);

#endif
