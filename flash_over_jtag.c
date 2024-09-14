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
* Description:       Main application module
*
* Modules Included:
*	void speed_test_32k(void);
*	void usage(void);
*	void redirect_pport(char *text);
*	int handleoptions(int argc,char *argv[]);
*	int main (int argc,char *argv[]);
*	void sys_init(void);
*	void display_memory(mem_read_constants mem_read);
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
****************************************************************************/

/*
Changes: Beta version: changed user interface, added page erase option
		 beta 0.2: data count bug fixed (data were counted from start of flash insted of start address)
		gamma 0.1: added functionality to support multiple JTAG devices in daisy-chain
		gamma 0.2: added logging capability, support for additional S-record file & switching to int. flash in case EXTBOOT=1
		delta 0.1: changed the page-erase algorithm to erase all and only pages referenced from the S-record file
				   changed interface for selecting page erase/mass erase mode between main application and the JTAG library
				   changed interface for changing printer port address between main application and the JTAG library
				   added dump functionality (both to file & screen)
				   added access to flash info block
		delta 0.2: verification errors were reported on incremented address - corrected
		delta 0.3: data written to incorrect address when starting elsewhere than beginning of flash
		delta 0.4: corrected 807 bootflash mass erase (writing address 0 is NOT equivalent to writing address F800, it is the same on other 80x chips)
		delta 0.5: wait for DSP to come out of reset option added
		delta 0.6: processing of the additional S-rec file was incorrect (-t option) and since delta 0.1 the file was infact not processed at all due to new structure of main()
		epsilon 0.1: port access modified to use the ZLPORTIO driver
					 jtag_write & jtag_read function optimized to speed-up daisy-chain operations
					 silent mode added
					 fixed behaviour when the /RESET pin is left unconnected and the flash does not contain meaningful contents (SR register needs to be cleared)
					 minor bugs fixed
		epsilon 0.2: updated ZLPORTIO driver installation
					 added -d option
		epsilon 0.3: changed max file path length to "MAX_PATH" under Win32
		epsilon 0.4: added jump to 0 in case the /RESET line would not be connected to the target
		epsilon 0.5: fixed bug in page erasing algorithm (variable addr was initialised with incorrect value)
		epsilon 0.6: jtag_data_read16 was optimized by commenting out some code, but TMS was not set correctly at the end of the communication in daisy-chain environments
					 changed daisy-chain info printout to make sure the information appears in the log file (in case one is being created)
		epsilon 0.7: added -c option (ignore S-rec checksum errors)
*/

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "flash.h"
#include "jtag.h"
#include "flash_over_jtag.h"
#include "srec.h"
#include "exit_codes.h"
#ifdef WIN32
	#include "port_io.h"
#endif

flash_constants flash_param[MAX_FLASH_UNITS];	/* constants for flash units */
int flash_count=0;								/* number of flash units */

operations operation=PROGRAM_FLASH;				/* what the tool is going to do: programming flash is default */

mem_read_constants mem_read;					/* parameters for reading memory */

#ifdef WIN32
    #define FILENAME_MAX_LEN	MAX_PATH
#endif
#ifdef MSDOS
    #define FILENAME_MAX_LEN	256
#endif
#ifdef linux
#include "limits.h"

#define FILENAME_MAX_LEN	PATH_MAX
#endif
char s_rec_filename[FILENAME_MAX_LEN+1]="";		/* name of the s-record file */
char cfg_filename[FILENAME_MAX_LEN+1]="";		/* name of the flash config file */
char timestamp_filename[FILENAME_MAX_LEN+1]="";	/* name of the additional S-record file to be processed */
char serror=0;									/* 0=report all errors, 1=silent mode (do not report all S-rec errors) */

void outp()
{
    return;
}

uint32_t inp()
{
    return 0;
}

/* the 32k test will write and verify whole program flash of 803/805 */
void speed_test_32k(void) {
	unsigned long t1,t2;
	int v;
	unsigned int start_addr=flash_param[0].flash_start;
    printf("Test start\r\n");
	t1=time(NULL);
	once_init_flash_iface(flash_param[0]);
	once_flash_mass_erase(flash_param[0]);
	once_flash_program_prepare (flash_param[0].interface_address, flash_param[0].flash_start);
	once_flash_program_pg_no(start_addr);
	for (v=0;v<32250;v++) {
		if (!(start_addr%32)) once_flash_program_pg_no(start_addr);
        if (!(v%512)) printf("p");
		once_flash_program_1word(flash_param[0], v);
		start_addr++;
	}
	once_flash_program_end();
    printf("\r\nProgramming done.\r\n");
	for (v=0;v<32250;v++) {
        if (!(v%512)) printf("v");
		once_flash_verify_1word(flash_param[0], v);
	}
    printf("\r\nVerification done.\r\n");
	jtag_disconnect();
	t2=time(NULL);
    printf("Test done\r\n");
    printf("\r\nTest results :\r\n");
    printf("Time for mass erase, programming & verification of 32250 words : %ld sec\r\n",t2-t1);
}

/* displays contents of memory on screen */
void display_memory(mem_read_constants mem_read) {
	unsigned int offset;
	int i;
	char c;
    printf("%s memory dump - 0x%04X:0x%04X\r\n\n",mem_read.program_memory?"Program":"Data",mem_read.start,mem_read.end);
	offset=0;
	while (offset<(mem_read.end-mem_read.start+1)) {
        printf("%c:%04X: ",mem_read.program_memory?'p':'x',mem_read.start+offset);
		for (i=0;i<8;i++) {
            if (mem_read.start+offset+i<=mem_read.end) printf("%04X ",*(mem_read.data+offset+i));
                else printf("       ");
		}
		for (i=0;i<8;i++) {
			if (mem_read.start+offset+i<=mem_read.end) {
				c=(*(mem_read.data+offset+i)&0xff00)>>8;	/* upper character */
                printf("%c",((c>' ')&&(c<127))?c:' ');
				c=(*(mem_read.data+offset+i)&0x00ff);		/* lower character */
                printf("%c",((c>' ')&&(c<127))?c:' ');
            } else printf("  ");
		}
        printf("\r\n");
		offset+=8;
	}
}

void sys_init(void) {
	int i;
	for (i=0;i<MAX_FLASH_UNITS;i++) {
		flash_param[i].data=NULL;
		flash_param[i].page_erase_map=NULL;
	}
	mem_read.data=NULL;
}

void cleanup(void) {
	int i;
	for (i=0;i<flash_count;i++) {
		if (flash_param[i].data!=NULL) free(flash_param[i].data);
		if ((!flash_param[i].duplicate)&&(flash_param[i].page_erase_map!=NULL)) free(flash_param[i].page_erase_map);
	}
	jtag_disconnect();
	#ifdef WIN32
		zliostop();
	#endif
}

void usage(void) {
    printf("\r\nUsage:\r\n\nFlash_over_JTAG <flash config file> <S-record file> [<options>] or\r\n");
    printf("Flash_over_JTAG <flash config file> [<options>]\r\n\n");
    printf("Options:\r\n\n");
    printf("-pPORT\tPORT specifies the printer port address, the default is 0x378 (LPT1)\r\n");
    printf("-w\tWait for the DSP to leave the Reset state or power-up\r\n");
    printf("-s\tSilent mode - S-rec file errors are not reported\r\n");
    printf("-d\tLeave the target in debug mode on exit\r\n");
    printf("-c\tIgnore checksum errors in the S-rec files\r\n");
    printf("-page\tSpecifies that page erases should be used instead of mass erase\r\n");
    printf("-info\tAccess information blocks of Flash units instead of main blocks\r\n");
    printf("-mI,D\tSupport for JTAG daisy-chain. I and D specify position in the chain\r\n");
    printf("-l<log file>\t\tWrite messages into log file\r\n");
    printf("-t<S-rec file>\t\tProcess additional S-record file\r\n");
    printf("-r<mem><start>:<end>\tDump DSP memory to S-record file\r\n");
    printf("-v<mem><start>:<end>\tDump DSP memory to screen\r\n");
    printf("\n");
}

void redirect_pport(char *text) {
	int portnum;
	if (!sscanf(text,"-p0x%x ",&portnum)) return;
	set_port(portnum);
    printf("Using printer port at address 0x%X.\r\n",portnum);
}

/* returns number of parameters or negative number in case of error */
int handleoptions(int argc,char *argv[]) {
	int i,notoption=0;
	for (i=1; i< argc;i++) {
		if (argv[i][0] == '/' || argv[i][0] == '-') {
			switch (argv[i][1]) {
				case '?':	/* -? */
					usage();
					break;
				case 'c':	/* -i - ignore S-rec checksum errors */
				case 'C':
					srec_check_checksums(0);
					break;
				case 't':	/* -t<S-record file> */
				case 'T':
					strcpy(timestamp_filename,argv[i]+2);
					break;
				case 'h':	/* -help */
				case 'H':
                    if (!strcmp(argv[i]+1,"help")) {
						usage();
						break;
					}
				case 'p':
				case 'P':
                    if (!strcmp(argv[i]+1,"page")) {
						set_erase_mode(1);
                        printf("Using Page Erase mode.\r\n");
						break;
					} else {	/* -pPORT */
						argv[i][0]='-';
						redirect_pport(argv[i]);
						break;
					}
				case 'i':
				case 'I':
                    if (!strcmp(argv[i]+1,"info")) {
						set_info_block(1);
                        printf("Flash Information Block access.\r\n");
					}
					break;
				case 'm':
				case 'M':	{	/* -mI,D */
						int instr,data;
						sscanf(argv[i]+2,"%d,%d",&instr,&data);
						/* the printout is done after all parameters are processed to make sure it appears in the log */
                        //printf("Target at position %d of instruction chain and %d of data chain.\r\n",instr,data);
						set_data_pp(data);
						set_instr_pp(instr);
						break;
					}
				case 'v':
				case 'V':		/* view memory */
					operation=VIEW_MEMORY;	/* the set-up is the same as for READ_MEMORY */
				case 'r':
				case 'R':	{	/* read memory */
						char mem_type;
						if ((argv[i][1]=='r')||(argv[i][1]=='R')) operation=READ_MEMORY;
						sscanf(argv[i]+2,"%c0x%x:0x%x",&mem_type,&(mem_read.start),&(mem_read.end));
						switch (mem_type) {
							case 'x':
							case 'X':
								mem_read.program_memory=0;	/* data memory */
								break;
							case 'p':
							case 'P':
								mem_read.program_memory=1;	/* program memory */
								break;
							default:
                                printf("Incorrect memory type\r\n");
								return(-1);
						}
						if (mem_read.end<mem_read.start) {
                            printf("Address range incorrect\r\n");
							return(-1);
						}
						mem_read.data=(unsigned int*)calloc(mem_read.end-mem_read.start+1,sizeof(unsigned int));
						if (mem_read.data==NULL) {
                            printf("Memory allocation error\r\n");
							return(-1);
						}
						break;
					}
				case 's':
				case 'S':
					serror=1;	/* do not report S-rec errors */
					break;
				case 'w':
				case 'W':		/* wait for the DSP to come out of reset */
					set_DSP_wait(1);
					break;
				case 'd':
				case 'D':
					set_exit_mode(1);	/* leave the part in debug mode on exit */
					break;
				default:
                    printf("Unknown option %s\n",argv[i]);
					break;
			}
		} else {
			if (notoption==0) {	/* first parameter which is not an option is the flash config file */
				strncpy(cfg_filename,argv[i],FILENAME_MAX_LEN);
			}
			if (notoption==1) { /* second parameter which is not an option is the S-record file */
				strncpy(s_rec_filename,argv[i],FILENAME_MAX_LEN);
			}
			if (notoption>=2) {
                printf("Too many parameters!\r\n");
			}
			notoption++;
		}
	}
	return(notoption);
}

/* Main program */
int main (int argc,char *argv[]) {
	int i;
	int parcount;
    printf("DSP56F800 Flash loader. Compiled on %s, %s.\r\n",__DATE__,__TIME__);
    printf("version Epsilon 0.7\r\n");
    printf("(c) Motorola 2001 - 2002, MCSL\r\n");
    printf("Partial Copyright 2000-2002, Zloba Alexander\r\n");
	#ifdef WIN32
		if (!zlioinit()) {
            printf("Unable to access the I/O driver\r\n");
			return(SYSTEM_ERROR);
		}
		if (!zliosetiopm(1)) {
            printf("Unable to gain direct access to I/O ports\r\n");
			return(SYSTEM_ERROR);
		}
	#endif
	i=atexit(cleanup);						/* install at exit function to clean up system */
	if (i!=0) {
        printf("Error setting \"at exit\" function\r\n");
		return(SYSTEM_ERROR);
	}
	sys_init();								/* init system variables */
	parcount=handleoptions(argc,argv);
	if ((parcount < 1) || (parcount > 2)) {	/* number of parameters is incorrect */
        printf("Number of parameters incorrect or other error\r\n");
		usage();
		return(PARAM_ERROR);
	}
	if (jtag_init()) {
        printf("Command Converter not connected or disabled!");
		return(JTAG_ERROR);
	}
	if (get_data_pp()||get_instr_pp()) {	/* if in daisy-chained environment, print target position */
        printf("Target at position %d of instruction chain and %d of data chain.\r\n",get_instr_pp(),get_data_pp());
	}
	if (init_target()) return(DSP_ERROR);
	switch (operation) {
		case PROGRAM_FLASH:
			if ((flash_count=read_setup(cfg_filename,flash_param))<0) return(CFG_ERROR);		/* read the flash config file */
			if (flash_prepare(flash_param,flash_count)) return(CFG_ERROR);						/* allocate memory */
			if (parcount==1) {
				speed_test_32k();
				return(SPEED_TEST_OK);
			}
			if (read_s_record(s_rec_filename, flash_param, flash_count, &serror)) return(SREC_ERROR);	/* read the input file, if error reading the file return -2 */
			if (timestamp_filename[0]) {
                printf("Processing timestamp file: %s\r\n",timestamp_filename);					/* if the filename is not null, process additional S-rec file */
				read_s_record(timestamp_filename, flash_param, flash_count, &serror);
			}
			for (i=0;i<flash_count;i++) if (once_flash_program(flash_param[i])) return(VERIFY_ERROR);
			return(SUCESS);
		case READ_MEMORY:
			if ((flash_count=read_setup(cfg_filename,flash_param))<0) return(CFG_ERROR);		/* read the flash config file */
			once_flash_read(mem_read.program_memory, mem_read.start, mem_read.end, mem_read.data,flash_param,flash_count);
			if (strlen(s_rec_filename)==0) write_s_record(cfg_filename, mem_read); else
				write_s_record(s_rec_filename, mem_read);	/* if only one file specified, use ir as S-record filename */
            printf("Output written.\r\n");
			break;
		case VIEW_MEMORY:
			if ((flash_count=read_setup(cfg_filename,flash_param))<0) return(CFG_ERROR);		/* read the flash config file */
			once_flash_read(mem_read.program_memory, mem_read.start, mem_read.end, mem_read.data,flash_param,flash_count);
			display_memory(mem_read);
			break;
	}
	return(SUCESS);
}
