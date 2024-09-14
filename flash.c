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
* Description:       Flash config file processing
*
* Modules Included:
*	int read_setup(char *path, flash_constants flash_param[])
*	int flash_prepare(flash_constants flash_param[], int flash_count)
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "flash.h"

/* Reads flash set-up from disk file */
/* returns number of flash blocks on success, -1 on file error */
int read_setup(char *path, flash_constants flash_param[]) {
	int i,base,j,k;
	FILE *input;
	char line[MAX_LINE_LENGTH+1];
	input=fopen(path,"r");
	if (input==NULL) {
        printf("Cannot open file \"%s\"\r\n",path);
		return(-1);
	}
	i=0;
	while ((i<MAX_FLASH_UNITS)&&(!feof(input))) {
		if (fgets(line,MAX_LINE_LENGTH,input)==NULL) break;
		if (line[0]!='#') {
			j=sscanf(line,"%d 0x%x 0x%x %d 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\r\n",&base,&(flash_param[i].flash_start),\
				&(flash_param[i].flash_end),&(flash_param[i].program_memory),&(flash_param[i].interface_address),\
				&(flash_param[i].terasel),&(flash_param[i].tmel),&(flash_param[i].tnvsl),&(flash_param[i].tpgsl),&(flash_param[i].tprogl),\
				&(flash_param[i].tnvhl),&(flash_param[i].tnvhl1),&(flash_param[i].trcvl),&(flash_param[i].clk_divisor));
			if (j==13) flash_param[i].clk_divisor=15;
			flash_param[i].data=NULL;
			flash_param[i].page_erase_map=NULL;
			flash_param[i].duplicate=0;
			for (k=0;k<i;k++) if (flash_param[i].interface_address==flash_param[k].interface_address) {
				flash_param[i].duplicate=1;
				break;
			}
			if (j>12) i++;
		}
	}
    printf("%d flash blocks defined in the config file.\r\n",i);
	fclose(input);
	return(i);
}

/* prepares memory for flash blocks */
/* allocatest memory and fills data buffers */
/* returns -1 on error */
int flash_prepare(flash_constants flash_param[], int flash_count) {
	long int i;
	unsigned long int addr;

	for (i=0;i<flash_count;i++) {
		/* allocate mem */
		flash_param[i].data=(unsigned int*)calloc(flash_param[i].flash_end-flash_param[i].flash_start+1,sizeof(unsigned int));
		if (flash_param[i].data==NULL) {
            printf("Memory allocation error for flash block #%d\r\n",i);
			return(-1);
		}
		for(addr=0;addr<(flash_param[i].flash_end-flash_param[i].flash_start+1);addr++) *(flash_param[i].data+addr)=65535;
		if (!flash_param[i].duplicate) {	/* if not duplicate, allocate new erase map */
			flash_param[i].page_erase_map=(unsigned int*)calloc(MAX_PAGE_COUNT,sizeof(unsigned int));
			if (flash_param[i].page_erase_map==NULL) {
                printf("Memory allocation error for flash block #%d\r\n",i);
				return(-2);
			}
			for(addr=0;addr<MAX_PAGE_COUNT;addr++) *(flash_param[i].page_erase_map+addr)=0;
		} else {
			addr=0;							/* if duplicate, find the original and assign the same map */
			while ((addr<i)&&(flash_param[i].interface_address!=flash_param[addr].interface_address)) addr++;
			flash_param[i].page_erase_map=flash_param[addr].page_erase_map;
		}
	}
	return(0);
}

