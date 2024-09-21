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
* Description:       S-record file processing
*
* Modules Included:
*	unsigned int hex2dec(char *bcd);
*	int s_line_process(char *line, unsigned long int *addr, unsigned int *data);
*	int find_flash(unsigned long int addr, flash_constants flash_param[], int flash_count);
*	int place_data(unsigned long int addr, unsigned int data, flash_constants flash_param[], int flash_count);
*	int read_s_record(char *path, flash_constants flash_param[], int flash_count);
*	int write_s_record(char *path, mem_read_constants mem_read);
*	void s_line_write(FILE *output, int type, int length, long int addr, void *data);
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "flash.h"
#include "flash_over_jtag.h"
#include "srec.h"

char checksums=1;	/* check checksums by default */

/* 1= perfor checks, 0= ignore checksums */
void srec_check_checksums(char i) {
    checksums=i;
}

/* converts 2 consecutive BCD numbers to unsigned integer value */
unsigned int hex2dec(char *bcd) {
    unsigned int i,result;
    int charvalue;
    result=0;
    for (i=0;i<2;i++) {
        charvalue=*bcd-'0';
        if (charvalue>9) charvalue-='A'-'0'-10;
        if (charvalue>15) charvalue-="a"-"A";
        if ((charvalue>15) || (charvalue<0)) {
            printf("Hex2dec conversion error: %c\n",*bcd);
            return (0);
        }
        bcd++;
        result*=16;
        result+=charvalue;
    }
    return (result);
}

/* Processes line of the S-record file */
/* returns: -1: format error, -2: line does not contain data, >0: number of converted data words */
/* checksum is ignored !!! */
int s_line_process(char *line, unsigned long int *addr, unsigned int *data) {
    int i,length,sum;
    if (line[0]!='S') {
        printf("S-record file line does not start with \"S\"\n");
        return (-1);
    }
    sum=length=hex2dec(line+2);
    if (line[1]=='3') {
        *addr=0;
        for (i=4;i<12;i+=2) {
            (*addr)*=256;
            (*addr)+=hex2dec(line+i);
            sum+=hex2dec(line+i);
            sum%=256;
        }
        for(i=12;i<((length+1)*2);i+=4) {
            *(data++)=hex2dec(line+i)+256*hex2dec(line+i+2);
            sum+=hex2dec(line+i);
            sum+=hex2dec(line+i+2);
            sum%=256;
        }
        sum+=1+hex2dec(line+(length+1)*2);
        sum%=256;
        if (sum) {
            printf("S-record checksum error\n");
            if (checksums) return (-1);
            printf("Error ignored\n");
        }
        return((length-5)/2);
    }
    if (line[1]=='0') {
        char c;
        printf("S-record ID: ");
        for(i=4;i<((length+1)*2);i+=2) {
            c=hex2dec(line+i);
            if((c>=32)&&(c<=127)) printf("%c",c);	/* print the character only if printable */
            sum+=c;
            sum%=256;
        }
        printf("\n");
        sum+=1+hex2dec(line+(length+1)*2);
        sum%=256;
        if (sum) {
            printf("S-record checksum error\n");
            if (checksums) return (-1);
            printf("Error ignored\n");
        }
        return(-2);
    }
    return(-2);
}

/* finds correct flash block for an input address */
/* returns number of flash block or -1 if not found */
int find_flash(unsigned long int addr, flash_constants flash_param[], int flash_count) {
    int i;
    for (i=0;i<flash_count;i++) {
        if (((addr%65536)>=flash_param[i].flash_start) && ((addr%65536)<=flash_param[i].flash_end)) {
            if ((addr>65536) && (!flash_param[i].program_memory)) break;
            if ((addr<65536) && (flash_param[i].program_memory)) break;
        }
    }
    if (i<flash_count) return(i);
    return(-1);
}

/* places data at correct location into buffers of flash inits */
/* returns: 0: OK, -1: no flash found for the address */
int place_data(unsigned long int addr, unsigned int data, flash_constants flash_param[], int flash_count) {
    int i;
    i=find_flash(addr, flash_param, flash_count);
    if (i>=0) {
        *(flash_param[i].data+(addr%65536)-flash_param[i].flash_start)=data;
        *(flash_param[i].page_erase_map+(((addr%65536)-flash_param[i].flash_start)/256))=2;
    } else return(-1);
    return(0);
}

/* Reads s-record file */
/* returns 0 on success, -1 on file error */
int read_s_record(char *path, flash_constants flash_param[], int flash_count, char *serror) {
    FILE *input;
    char line[MAX_LINE_LENGTH+1];
    unsigned int line_data[MAX_WORDS_PER_LINE];
    unsigned long int addr;
    long int i;
    int j;
    input=fopen(path,"r");
    if (input==NULL) {
        printf("Cannot open file \"%s\"\n",path);
        return(-1);
    }
    while (!feof(input)) {
        fgets(line,MAX_LINE_LENGTH,input);
        j=s_line_process(line,&addr,line_data);
        if (j>0) for (i=0;i<j;i++) if (place_data(addr+i,line_data[i], flash_param, flash_count)) {
                    if ((*serror)==1) {
                        printf("Some data ignored, details not reported (silent mode)\n");
                        (*serror)++;
                    } else if (*serror==0) printf("Data @ 0x%X ignored\n",addr%65536);
                }
    }
    for (j=0;j<flash_count;j++) {
        i=0;
        while((*(flash_param[j].data+i)==65535) && (i<flash_param[j].flash_end-flash_param[j].flash_start)) i++;
        flash_param[j].start_addr=flash_param[j].flash_start+i;
        i=flash_param[j].flash_end-flash_param[j].flash_start;
        while((*(flash_param[j].data+i)==65535) && (i>=0)) i--;
        if (i<0) flash_param[j].data_count=0; else flash_param[j].data_count=i+1-(flash_param[j].start_addr-flash_param[j].flash_start);
    }
    return(0);
}

/* writes s-record line to output file */
/* type: 0 header, 3 data, 7 end record */
/* length: number of bytes (header) or words (data) to output */
void s_line_write(FILE *output, int type, int length, long int addr, void *data) {
    int i,j,sum;
    fputc('S',output);
    fputc('0'+type,output);
    switch (type) {
    case 0:
        sum=length+1;
        fprintf(output,"%02X",sum);
        for (i=0;i<length;i++) {
            sum+=*((char*)data);
            sum%=256;
            fprintf(output,"%02X",*((char*)data));
            data=(void*)(((char*)data)+1);
        }
        break;
    case 3:
        sum=length*2+5;		/* words-> *2, address & checksum = 5 bytes */
        fprintf(output,"%02X",sum);
        for (i=0;i<4;i++) {	/* address */
            j=(addr&0xff000000)>>24;
            addr=(addr*256)&0xffffffff;
            fprintf(output,"%02X",j);
            sum+=j;
            sum%=256;
        }
        for (i=0;i<length;i++) {	/* data */
            /* lower byte first */
            j=*((unsigned int*)data)&0xff;
            sum+=j;
            fprintf(output,"%02X",j);
            j=*((unsigned int*)data)/256;	/* lower byte */
            sum+=j;
            sum%=256;
            fprintf(output,"%02X",j);
            data=(void*)(((unsigned int*)data)+1);
        }
        break;
    case 7:	fprintf(output,"0500000084");
        sum=0xff-0x76;
    }
    fprintf(output,"%02X\n",0xff-sum); /* checksum */
}

/* Creates s-record file */
/* returns 0 on success, -1 on file error */
int write_s_record(char *path, mem_read_constants mem_read) {
    FILE *output;
    char header[35];
    unsigned long int count,i;
    output=fopen(path,"wb");
    if (output==NULL) {
        printf("Cannot create file \"%s\"\n",path);
        return(-1);
    }
    sprintf(header,"%s memory dump 0x%X:0x%X",mem_read.program_memory?"Program":"Data",mem_read.start,mem_read.end);
    s_line_write(output, 0, strlen(header), 0, header);
    count=mem_read.end-mem_read.start+1;
    i=0;
    while (i<count) {
        s_line_write(output,
                     3,
                     ((count-i)>OUTPUT_S_REC_DATA_PER_LINE)?OUTPUT_S_REC_DATA_PER_LINE:(count-i),
                     mem_read.start+i+(mem_read.program_memory?0:0x200000), mem_read.data+i);
        i+=OUTPUT_S_REC_DATA_PER_LINE;
    }
    s_line_write(output, 7, 0, 0, NULL);
    fclose(output);
    return(0);
}

