/*****************************************************************************
*
* Motorola Inc.
* (c) Copyright 2001,2002 Motorola, Inc.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* File Name:         flash_over_jtag.h
*
* Description:       Main application prototypes
*
* Modules Included:  None
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
****************************************************************************/

#ifndef FLASH_OVER_JTAG____H
#define FLASH_OVER_JTAG____H

typedef enum {
    PROGRAM_FLASH,
    READ_MEMORY,
    VIEW_MEMORY,
} operations;

typedef struct {
	unsigned int	start;		/* beginning of the block */
	unsigned int	end;		/* end of the block */
	unsigned char	program_memory;	/* 1-pflash, 0-dflash */
	unsigned int	*data;		/* pointer to array where data are to be stored */
} mem_read_constants;

void sys_init(void);
void cleanup(void);
void usage(void);
int handleoptions(int argc,char *argv[]);
int main (int argc,char *argv[]);
void display_memory(mem_read_constants mem_read);


#endif
