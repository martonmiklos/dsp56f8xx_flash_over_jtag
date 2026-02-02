/*****************************************************************************
*
* Motorola Inc.
* (c) Copyright 2001,2002 Motorola, Inc.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
*
* File Name:         jtag.c
*
* Description:       Jtag and OnCE access
*
* Modules Included:
*	int jtag_init(void);
*	int jtag_instruction_exec(int instruction);
*	int jtag_instruction_exec_in_reset(int instruction);
*	unsigned long int jtag_data_shift(unsigned long int data, int bit_count);
*	int init_target (void);
*	int once_init_flash_iface(flash_constants flash_param);
*	void once_flash_program_prepare (unsigned int fiu_address, unsigned int addr);
*	void once_flash_program_pg_no (unsigned int addr);
*	void once_flash_program_end (void);
*	void once_flash_program_1word(flash_constants flash_param, unsigned int data);
*	int once_flash_verify_1word(flash_constants flash_param, unsigned int data);
*	int once_flash_mass_erase(flash_constants flash_param);
*	int once_flash_page_erase(flash_constants flash_param);
*	int once_flash_program(flash_constants flash_param);
*	void jtag_disconnect(void);
*	void jtag_data_write8(unsigned int data);
*	void jtag_data_write16(unsigned int data);
*	unsigned int jtag_data_read16(void);
*	void jtag_measure_paths(void);
*	int get_data_pl(void);
*	int get_instr_pl(void);
*	int get_data_pp(void);
*	int get_instr_pp(void);
*	void set_data_pp(int length);
*	void set_instr_pp(int length);
*	void once_flash_read_prepare (unsigned int addr, flash_constants flash_param[], int flash_count);
*	unsigned int once_flash_read_1word(unsigned char program_memory);
*	void once_flash_read(unsigned char program_memory, unsigned int start_addr, unsigned int end_addr, unsigned int *buffer, flash_constants flash_param[], int flash_count);
*	void set_erase_mode(unsigned char mode)
*	void set_port(unsigned int port);
*	void set_info_block(unsigned int value);
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
****************************************************************************/

#include <ftdi.h>

#include "hw_access.h"
#include "flash.h"
#include "jtag.h"
#include <stdio.h>
#include <stdbool.h>

unsigned int pport_data=0;						/* mirror of output port to save accesses */

unsigned char page_erase=0;						/* 1: page erase, 0: mass erase */

unsigned char info_block=0;						/* 1: info block, 0: normal flash access */

unsigned char wait_for_DSP=0;					/* 1: wait for DSP to come out of reset (external reset circuit or power down/up for 801 bootloader erasure) */

unsigned char exit_mode=0;	/* ==0 - reset the target, !=0 - leave in debug mode */

int data_pl;		/* lengths of JTAG paths */
int instr_pl;
int data_pp=0;		/* position of the part in the JTAG chain, 0=beginning */
int instr_pp=0;

struct ftdi_context *ftdic = NULL;
bool ftdi_open = false;

/* set info block (1) or normal access (0) mode */
void set_info_block(unsigned int value) {
    info_block=value;
}

/* port number */
int open_port() {
    ftdic = ftdi_new();
    if (!ftdic)
        return -1;

    ftdi_init(ftdic);

    if (ftdi_usb_open(ftdic, 0x0403, 0x6014) != 0) { // FT232H adapt the PID if needed
        printf("Unable to open FT232H\n");
        return -1;
    }
    ftdi_open = true;

    if (ftdi_set_bitmode(ftdic,
                         JTAG_RESET_MASK |
                             JTAG_TMS_MASK |
                             JTAG_TCK_MASK |
                             JTAG_TDI_MASK |
                             JTAG_TRST_MASK,
                         BITMODE_BITBANG) != 0) {
        ftdi_usb_close(ftdic);
        return -1;
    }
    return 0;
}

void jtag_outp(uint8_t data)
{
    ftdi_write_data(ftdic, &data, 1);
    return;
}

uint8_t jtag_inp()
{
    uint8_t ret = 0;
    int rc = ftdi_read_pins(ftdic, &ret);
    if (rc != 0)
        printf("ftdi_read_pins failed with: %d\n", rc);
    return ret;
}


/* set mass erase (0) or page_erase (1) mode */
void set_erase_mode(unsigned char mode) {
    if (mode) page_erase=1; else page_erase=0;
}

/* routines for handling data path length variables */
int get_data_pl(void) {
    return(data_pl);
}
int get_instr_pl(void) {
    return(instr_pl);
}
int get_data_pp(void) {
    return(data_pp);
}
int get_instr_pp(void) {
    return(instr_pp);
}
void set_data_pp(int length) {
    data_pp=length;
}
void set_instr_pp(int length) {
    instr_pp=length;
}

void set_DSP_wait(char wait) {
    wait_for_DSP=wait;
}

void set_exit_mode(unsigned char mode) {
    exit_mode=mode;
}

/* measures data and instruction JTAG path lengths */
/* expects Select-DR-Scan state of the Jtag state machine state upon entry */
/* and leaves the Jtag in Select-DR-Scan on exit */
/* returns 0 in case measurement has overflown, 1 in case measurement is OK */
int jtag_measure_paths(void) {
    int i;
    JTAG_TMS_SET;								/* Go to Select-IR-Scan */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;
    JTAG_TMS_RESET;								/* Go to Capture-IR */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;
    JTAG_TCK_RESET;
    JTAG_TCK_SET;								/* Go to Shift-IR */ /* Now the Jtag is in the Shift-IR state */
    JTAG_TDI_ASSIGN(1);
    for (i=0;i<JTAG_PATH_LEN_MAX;i++) {
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    JTAG_TDI_ASSIGN(0);								/* shift 0 into beginning of the IR path */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;
    JTAG_TDI_ASSIGN(1);
    i=0;
    while ((JTAG_TDO_VALUE)&&(i<JTAG_PATH_LEN_MAX)) { /* wait for the 0 to appear at the path end */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
        i++;
    }
    JTAG_TCK_RESET;
    JTAG_TCK_SET;									/* the whole IR path now contains 1 = BYPASS command */
    JTAG_TMS_SET;
    JTAG_TCK_RESET;									/* Go to Exit1-IR */
    JTAG_TCK_SET;
    JTAG_TCK_RESET;									/* Go to Update-IR */
    JTAG_TCK_SET;
    JTAG_TCK_RESET;									/* Go to Select-DR-Scan */
    JTAG_TCK_SET;
    instr_pl=i;

    JTAG_TMS_RESET;								/* Go to Capture-DR */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;								/* Go to Shift-DR */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;								/* Now the Jtag is in the Shift-DR state */
    JTAG_TDI_ASSIGN(1);
    for (i=0;i<JTAG_PATH_LEN_MAX;i++) {
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    JTAG_TDI_ASSIGN(0);								/* shift 0 into beginning of the DR path */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;
    JTAG_TDI_ASSIGN(1);
    i=0;
    while ((JTAG_TDO_VALUE)&&(i<JTAG_PATH_LEN_MAX)) { /* wait for the 0 to appear at the path end */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
        i++;
    }
    JTAG_TCK_RESET;
    JTAG_TCK_SET;								/* the whole DR path now contains 1 */
    JTAG_TMS_SET;
    JTAG_TCK_RESET;								/* Go to Exit1-DR */
    JTAG_TCK_SET;
    JTAG_TCK_RESET;								/* Go to Update-DR */
    JTAG_TCK_SET;
    JTAG_TCK_RESET;								/* Go to Select-DR-Scan */
    JTAG_TCK_SET;
    data_pl=i;
    if ((data_pl<JTAG_PATH_LEN_MAX)&&(instr_pl<JTAG_PATH_LEN_MAX)) return(1); else return(0);
}

/* initialises JTAG, but leaves the part int reset */
/* the DSP is brought out of reset in "init_target" routine */
int jtag_init(void) {
    long int i;
    INIT_WAIT_FUNCTION;
    WAIT_100_NS;								/* wait for power to stabilise */
    WAIT_100_NS;
    WAIT_100_NS;
    WAIT_100_NS;
    JTAG_TCK_SET;
    JTAG_TMS_SET;
    JTAG_TRST_SET;
    JTAG_RESET_SET;
    JTAG_TDI_RESET;								/* all JTAG signals are reset to known state */


    /* reset JTAG on the target */
    /* The TRST pin does not neccessarily have to be connected, in such case we expect
	the JTAG state machine to be already reset to Test-Logic-Reset state */

    JTAG_TRST_RESET;								/* /TRST signal goes low */
    JTAG_RESET_RESET;								/* /RESET signal goes low */
    WAIT_100_NS;
    WAIT_100_NS;
    WAIT_100_NS;
    WAIT_100_NS;
    JTAG_TRST_SET;									/* /TRST signal goes high */
    //	JTAG_RESET_SET;									/* /RESET signal goes high */
    //	WAIT_100_NS;
    //	WAIT_100_NS;
    //	WAIT_100_NS;
    //	WAIT_100_NS;
    for (i=0;i<10;i++) {
        JTAG_TCK_RESET;			/* TMS must be sampled as '1' at least 5 times after power-up */
        JTAG_TCK_SET;			/* plus 5 more times to bring the target to Test-Logic-Reset in case /TRST is not connected */
    }
    JTAG_TMS_RESET;
    JTAG_TCK_RESET;									/* Go to Run-Test-Idle */
    JTAG_TCK_SET;
    JTAG_TMS_SET;									/* Go to Select-DR-Scan */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;
    return(0);
}

/* resets the DSP core by asserting /RESET and executes the JTAG instruction */
/* useful for bringing the target into debug mode when flash contains errorneous code */
int jtag_instruction_exec_in_reset(int instruction) {
    int i,status=0;
    JTAG_RESET_RESET;							/* /RESET signal goes low */
    WAIT_100_NS;
    WAIT_100_NS;
    WAIT_100_NS;
    WAIT_100_NS;
    JTAG_TMS_SET;								/* Go to Select-IR-Scan */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;
    JTAG_TMS_RESET;								/* Go to Capture-IR */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;
    JTAG_TCK_RESET;
    JTAG_TCK_SET;								/* Go to Shift-IR */ /* Now the Jtag is in the Shift-IR state */
    if (instr_pl-instr_pp-4) {
        JTAG_TDI_ASSIGN(1);
    }

    for(i=0;i<(instr_pl-instr_pp-4);i++) {
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }

    for (i=0;i<4;i++) {
        JTAG_TDI_ASSIGN(instruction);
        instruction>>=1;
        if ((instr_pp==0)&&(i==3)) JTAG_TMS_SET;	/* Go to Exit1-IR */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
        status>>=1;
        status|=JTAG_TDO_VALUE<<3;
    }
    if (instr_pp) JTAG_TDI_ASSIGN(1);
    for (i=0;i<instr_pp;i++) {
        if (i==(instr_pp-1)) JTAG_TMS_SET;			/* Go to Exit1-IR */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    JTAG_TCK_RESET;								/* Go to Update-IR */
    JTAG_TCK_SET;
    JTAG_TCK_RESET;								/* Go to Select-DR-Scan */
    JTAG_TCK_SET;
    JTAG_RESET_SET;								/* /RESET signal goes high */
    for (i=0;i<50;i++) 	WAIT_100_NS;			/* wait 5us for the chip to come out of reset again */
    return(status);
}

/* Set all JTAG signals inactive and reset target DSP */
void jtag_disconnect(void) {
    if (ftdi_open) {
        JTAG_TCK_RESET;
        JTAG_TMS_RESET;
        JTAG_TDI_RESET;
        if (exit_mode==0) {
            JTAG_RESET_RESET;						/* /TRST & /RESET signals go low */
            once_jmp_run(0);						/* jump to address 0 in case the /RESET line would not be connected */
            JTAG_TRST_RESET;
            jtag_instruction_exec(0x2);				/* execute IDCODE in case the /TRST line would not be connected */
            WAIT_100_NS;
            WAIT_100_NS;
            JTAG_TRST_SET;							/* /TRST & /RESET signals go high */
            JTAG_RESET_SET;
            printf("The target was reset, the application is running\n");
        } else {
            jtag_instruction_exec(0x2);				/* execute IDCODE */
            JTAG_TRST_SET;							/* /TRST & /RESET signals go high */
            JTAG_RESET_SET;
            printf("The target was left in debug mode\n");
        }
        ftdi_usb_close(ftdic);
    }
    ftdi_free(ftdic);
}

/* Executes Jtag command */
/* expects Select-DR-Scan state of the Jtag state machine state upon entry */
/* and leaves the Jtag in Select-DR-Scan on exit */
int jtag_instruction_exec(int instruction) {
    int i,status=0;
    JTAG_TMS_SET;								/* Go to Select-IR-Scan */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;
    JTAG_TMS_RESET;								/* Go to Capture-IR */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;
    JTAG_TCK_RESET;
    JTAG_TCK_SET;								/* Go to Shift-IR */ /* Now the Jtag is in the Shift-IR state */
    if (instr_pl-instr_pp-4) JTAG_TDI_ASSIGN(1);
    for(i=0;i<(instr_pl-instr_pp-4);i++) {
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    for (i=0;i<4;i++) {
        JTAG_TDI_ASSIGN(instruction);
        instruction>>=1;
        if ((instr_pp==0)&&(i==3)) JTAG_TMS_SET;	/* Go to Exit1-IR */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
        status>>=1;
        status|=JTAG_TDO_VALUE<<3;
    }
    if (instr_pp) JTAG_TDI_ASSIGN(1);
    for (i=0;i<instr_pp;i++) {
        if (i==(instr_pp-1)) JTAG_TMS_SET;			/* Go to Exit1-IR */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    JTAG_TCK_RESET;								/* Go to Update-IR */
    JTAG_TCK_SET;
    JTAG_TCK_RESET;								/* Go to Select-DR-Scan */
    JTAG_TCK_SET;
    return(status);
}

/* shifts up to 32 bits in and out of the jtag DR path */
/* expects Select-DR-Scan state of the Jtag state machine state upon entry */
/* and leaves the Jtag in Select-DR-Scan on exit */
unsigned long int jtag_data_shift(unsigned long int data, int bit_count) {
    int i;
    unsigned long int result=0;
    JTAG_TMS_RESET;								/* Go to Capture-DR */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;								/* Go to Shift-DR */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;								/* Now the Jtag is in the Shift-DR state */
    if (data_pl-1-data_pp) JTAG_TDI_ASSIGN(1);
    for(i=0;i<(data_pl-1-data_pp);i++) {
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    for (i=0;i<bit_count;i++) {
        JTAG_TDI_ASSIGN(data);
        data>>=1;
        if ((data_pp==0)&&(i==(bit_count-1))) JTAG_TMS_SET;	/* Go to Exit1-DR */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
        result>>=1;
        result|=((unsigned long int)JTAG_TDO_VALUE)<<(bit_count-1);
    }
    if (data_pp) JTAG_TDI_ASSIGN(1);
    for (i=0;i<data_pp;i++) {
        if (i==(data_pp-1)) JTAG_TMS_SET;			/* Go to Exit1-DR */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    JTAG_TCK_RESET;								/* Go to Update-DR */
    JTAG_TCK_SET;
    JTAG_TCK_RESET;								/* Go to Select-DR-Scan */
    WAIT_100_NS;
    WAIT_100_NS;
    JTAG_TCK_SET;
    return(result);
}

/* brings target into Debug mode and enables the Once interface */
int init_target (void) {
    int status = 0, i = 0;
    unsigned long int result;
    jtag_measure_paths();				/* measure JTAG chain length */
    if (wait_for_DSP) {					/* we need to wait until the DSP powers-up or comes out of Reset */
        printf("Waiting for target board to power-up & DSP to come out of reset...\n");
        JTAG_RESET_SET;					/* /RESET signal goes high */
        while(status!=0x0d) {
            jtag_measure_paths();		/* measure again (in case the board had no power we need to get correct lengths) */
            JTAG_TRST_RESET;			/* /TRST signal goes low */
            WAIT_100_NS;
            WAIT_100_NS;
            WAIT_100_NS;
            WAIT_100_NS;
            JTAG_TRST_SET;				/* /TRST signal goes high */
            for (i=0;i<10;i++) {
                JTAG_TCK_RESET;			/* TMS must be sampled as '1' at least 5 times after power-up */
                JTAG_TCK_SET;			/* plus 5 more times to bring the target to Test-Logic-Reset in case /TRST is not connected */
            }
            JTAG_TMS_RESET;
            JTAG_TCK_RESET;				/* Go to Run-Test-Idle */
            JTAG_TCK_SET;
            JTAG_TMS_SET;				/* Go to Select-DR-Scan */
            JTAG_TCK_RESET;
            JTAG_TCK_SET;
            status=jtag_instruction_exec(0x7);	/*Debug Request*/
            switch (status) {
            case (0x00):
            case (0x0f):	printf("No power?          \n"); break;
            case (0x09):	printf("DSP in Reset       \n"); break;
            case (0x01):	printf("DSP running        \n"); break;
            case (0x05):	printf("DSP in Wait or Stop\n"); break;
            }
        }
    }
    printf("JTAG IR path length: %d\n",get_instr_pl());		/* print JTAG path lengths */
    printf("JTAG DR path length: %d (BYPASS)\n",get_data_pl());
    status=jtag_instruction_exec(0x2);			/*IDCODE*/
    printf("IDCode status: %#x\n",status);
    result=jtag_data_shift(0,32);
    printf("Jtag ID: %#lx\n",result);
    status=jtag_instruction_exec(0x7);			/*Debug Request*/
    /*if (!wait_for_DSP) {
        JTAG_RESET_SET;
        usleep(10);
    }
    status=jtag_instruction_exec(0x7);	*/	// Debug Request #2
    printf("Debug Request status: %#x\n",status);
    i=RETRY_DEBUG;
    do {
        status=jtag_instruction_exec(0x6);	/*Enable OnCE*/
        printf("Enable OnCE status: %#x, polls left: %d\n",status,i);
        if (!(i--)) {
            printf("Target chip refused to enter Debug mode!\n");
            return(1);
        }
    } while (status!=0xd);
    printf("Enable OnCE successful, target chip is in Debug mode\n");
    /* Now switch the memory map to internal flash (just in case EXTBOOT=1) */
    once_move_data_to_y0(0);		/* MOVE #0x0000,Y0 */
    once_move_y0_to_omr();			/* MOVE Y0,OMR */
    /* make sure the SR contains meaningful value (bits 10..14 must be 0) */
    once_jmp(0);					/* JMP #0x0000 - clear PC extension (bits 10..14 of SR) */
    once_move_data_to_y0(0x0300);	/* MOVE #0x0300,Y0 */
    once_move_y0_to_sr();			/* MOVE Y0,SR */
    return(0);
}

/* writes 8 bits to the JTAG DR path */
/* expects Select-DR-Scan state of the Jtag state machine state upon entry */
/* and leaves the Jtag in Select-DR-Scan on exit */
void jtag_data_write8(unsigned int data) {
    int i;
    JTAG_TMS_RESET;								/* Go to Capture-DR */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;
    JTAG_TCK_RESET;								/* Go to Shift-DR */
    JTAG_TCK_SET;								/* Now the Jtag is in the Shift-DR state */
    //	if (data_pl-1-data_pp) JTAG_TDI_ASSIGN(1);	// this is not needed for write only operation
    //	for(i=0;i<(data_pl-1-data_pp);i++) {
    //		JTAG_TCK_RESET;
    //		JTAG_TCK_SET;
    //	}
    for (i=0;i<8;i++) {
        JTAG_TDI_ASSIGN(data);
        data>>=1;
        if ((data_pp==0)&&(i==7)) JTAG_TMS_SET;		/* Go to Exit1-DR */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    if (data_pp) JTAG_TDI_ASSIGN(1);
    for (i=0;i<data_pp;i++) {
        if (i==(data_pp-1)) JTAG_TMS_SET;			/* Go to Exit1-DR */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    JTAG_TCK_RESET;								/* Go to Update-DR */
    JTAG_TCK_SET;
    JTAG_TCK_RESET;								/* Go to Select-DR-Scan */
    WAIT_100_NS;
    WAIT_100_NS;
    JTAG_TCK_SET;
}

/* writes 16 bits to the JTAG DR path */
/* expects Select-DR-Scan state of the Jtag state machine state upon entry */
/* and leaves the Jtag in Select-DR-Scan on exit */
void jtag_data_write16(unsigned int data) {
    int i;
    JTAG_TMS_RESET;								/* Go to Capture-DR */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;
    JTAG_TCK_RESET;								/* Go to Shift-DR */
    JTAG_TCK_SET;								/* Now the Jtag is in the Shift-DR state */
    //	if (data_pl-1-data_pp) JTAG_TDI_ASSIGN(1);	//this is not needed for write only operation
    //	for(i=0;i<(data_pl-1-data_pp);i++) {
    //		JTAG_TCK_RESET;
    //		JTAG_TCK_SET;
    //	}
    for (i=0;i<16;i++) {
        JTAG_TDI_ASSIGN(data);
        data>>=1;
        if ((data_pp==0)&&(i==15)) JTAG_TMS_SET;		/* Go to Exit1-DR */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    if (data_pp) JTAG_TDI_ASSIGN(1);
    for (i=0;i<data_pp;i++) {
        if (i==(data_pp-1)) JTAG_TMS_SET;			/* Go to Exit1-DR */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    JTAG_TCK_RESET;								/* Go to Update-DR */
    JTAG_TCK_SET;
    JTAG_TCK_RESET;								/* Go to Select-DR-Scan */
    WAIT_100_NS;
    WAIT_100_NS;
    JTAG_TCK_SET;
}

/* reads 16 bits from the jtag DR path */
/* expects Select-DR-Scan state of the Jtag state machine state upon entry */
/* and leaves the Jtag in Select-DR-Scan on exit */
unsigned int jtag_data_read16(void) {
    int i;
    unsigned int result=0;
    JTAG_TMS_RESET;								/* Go to Capture-DR */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;								/* Go to Shift-DR */
    JTAG_TCK_RESET;
    JTAG_TCK_SET;								/* Now the Jtag is in the Shift-DR state */
    if (data_pl-1-data_pp) {
        JTAG_TDI_ASSIGN(1);
    }
    for(i=0;i<(data_pl-1-data_pp);i++) {
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
    }
    for (i=0;i<16;i++) {
        //		if ((data_pp==0)&&(i==15)) JTAG_TMS_SET;	/* Go to Exit1-DR */
        if (i==15) JTAG_TMS_SET;					/* Go to Exit1-DR */
        JTAG_TCK_RESET;
        JTAG_TCK_SET;
        result>>=1;
        if (JTAG_TDO_VALUE)
            result |= 0x8000;
    }
    //	if (data_pp) JTAG_TDI_ASSIGN(1);			//this is not needed for read only operation
    //	for (i=0;i<data_pp;i++) {
    //		if (i==(data_pp-1)) JTAG_TMS_SET;		/* Go to Exit1-DR */
    //		JTAG_TCK_RESET;
    //		JTAG_TCK_SET;
    //	}
    JTAG_TCK_RESET;								/* Go to Update-DR */
    JTAG_TCK_SET;
    JTAG_TCK_RESET;								/* Go to Select-DR-Scan */
    WAIT_100_NS;
    WAIT_100_NS;
    JTAG_TCK_SET;
    return(result);
}

/* initialises the Flash Timing registers for Flash programming interface at given address */
int once_init_flash_iface(flash_constants flash_param) {
    printf("Initialising FIU at address: %#x\n",flash_param.interface_address);
    once_move_data_to_r2(flash_param.interface_address);	/* MOVE #<base address>,R2		*/
    once_move_data_to_y0(info_block?0x0040:0);	/* MOVE #0+IFREN,Y0				*/
    once_move_y0_to_xr2_inc();					/* clear FIU_CNTL register	*/
    if (info_block) once_move_data_to_y0(0);	/* MOVE #0,Y0				*/
    once_move_y0_to_xr2_inc();					/* clear FIU_PE register	*/
    once_move_y0_to_xr2_inc();					/* clear FIU_EE register	*/

    once_move_data_to_r1(flash_param.interface_address);	/* MOVE #<base address>,R1 	 */
    once_move_data_to_r0(flash_param.interface_address+8);	/* MOVE #<base address+8>,R0 */
    once_move_xr1_inc_to_y0();					/* MOVE x:R1,Y0				 	*/
    once_move_y0_to_xmem(0xffff);				/* MOVE Y0,<OPGDBR> 		 	*/
    if (once_opgdbr_read()&0x8000) {			/* Read OPGDBR register 		*/
        printf("FIU initialisation failed, BUSY bit is set.\n");
        return(1);
    }
    once_move_data_to_y0(flash_param.clk_divisor);			/* now fill the timing registers */
    once_move_y0_to_xr0_inc();
    once_move_data_to_y0(flash_param.terasel);
    once_move_y0_to_xr0_inc();
    once_move_data_to_y0(flash_param.tmel);
    once_move_y0_to_xr0_inc();
    once_move_data_to_y0(flash_param.tnvsl);
    once_move_y0_to_xr0_inc();
    once_move_data_to_y0(flash_param.tpgsl);
    once_move_y0_to_xr0_inc();
    once_move_data_to_y0(flash_param.tprogl);
    once_move_y0_to_xr0_inc();
    once_move_data_to_y0(flash_param.tnvhl);
    once_move_y0_to_xr0_inc();
    once_move_data_to_y0(flash_param.tnvhl1);
    once_move_y0_to_xr0_inc();
    once_move_data_to_y0(flash_param.trcvl);
    once_move_y0_to_xr0_inc();
    printf("FIU (%#x) initialisation done.\n", flash_param.interface_address);
    return(0);
}

/* prepares flash programming */
/* R0 = R2 = start address, R0 for writing, R2 for verification */
void once_flash_program_prepare (unsigned int fiu_address, unsigned int addr) {
    once_move_data_to_r0(addr);				/* MOVE #<address>,R0 		 */
    once_move_data_to_r1(fiu_address+1);	/* MOVE #<fiu_address+1>,R1  */
    once_move_data_to_r2(addr);				/* MOVE #<address>,R2 		 */
    once_move_data_to_r3(fiu_address);		/* MOVE #<fiu_address>,R3	 */
#ifdef DEBUG
    printf("\nDBG: FIU_ADDR: 0x%x",fiu_address);
    printf("\nDBG: START_ADDR: 0x%x",addr);
#endif
}

void once_flash_program_pg_no (unsigned int addr) {
#ifdef DEBUG
    printf("\nDBG: PG_NO: 0x%x",addr);
#endif
    do {
        once_move_xr3_to_y0();				/* MOVE x:R3,Y0				 */
        once_move_y0_to_xmem(0xffff);		/* MOVE Y0,<OPGDBR> 		 */
    }
    while (once_opgdbr_read()&0x8000);	/* repeat poll while BUSY is set */
    once_move_data_to_y0(0x4000 + (( addr >> 5) & 0x03ff));	/* MOVE #<pe>,Y0 */
    once_move_y0_to_xr1();					/* MOVE Y0,x:R1	(FIU_PE)	 */
}

void once_flash_program_end (void) {
    do {
        once_move_xr3_to_y0();				/* MOVE x:R3,Y0				 */
        once_move_y0_to_xmem(0xffff);		/* MOVE Y0,<OPGDBR> 		 */
    }
    while (once_opgdbr_read()&0x8000);	/* repeat poll while BUSY is set */
    once_move_data_to_y0(0);				/* MOVE #0,Y0				 */
    once_move_y0_to_xr1();					/* MOVE Y0,x:R1	(FIU_PE)	 */
}

/* programs one word */
void once_flash_program_1word(flash_constants flash_param, unsigned int data) {
#ifdef DEBUG
    printf("\nDBG: DATA: 0x%x",data);
#endif
    once_move_data_to_y1(data);				/* MOVE #<data>,Y1			 */
    do {
        once_move_xr3_to_y0();				/* MOVE x:R3,Y0				 */
        once_move_y0_to_xmem(0xffff);		/* MOVE Y0,<OPGDBR> 		 */
    }
    while (once_opgdbr_read()&0x8000);	/* repeat poll while BUSY is set */
    if (!(flash_param.program_memory))
    {
        once_move_y1_to_xr0_inc();
    }			/* MOVE Y0,x:R0	(data->x:addr)*/
    else
    {
        once_move_y1_to_pr0_inc();
    }			/* MOVE Y0,x:R0	(data->p:addr)*/
}

/* verification of one word */
int once_flash_verify_1word(flash_constants flash_param, unsigned int data) {
    unsigned int i;
    if (!(flash_param.program_memory))
    {
        once_move_xr2_inc_to_y0();
    }			/* MOVE x:R2,Y0	(x:addr)	 */
    else
    {
        once_move_pr2_inc_to_y0();
    }			/* MOVE p:R2,Y0 (p:addr)	 */
    once_move_y0_to_xmem(0xffff);				/* MOVE Y0,<OPGDBR> 			 */
    if ((i=once_opgdbr_read())!=data) {		/* Read OPGDBR register 	 */
        unsigned int addr;
        once_move_r2_to_y0();					/* MOVE R2,Y0		 			 */
        once_move_y0_to_xmem(0xffff);			/* MOVE Y0,<OPGDBR> 			 */
        addr=once_opgdbr_read();
        printf("Verification error at addr: %#x, wr: %#x, rd: %#x\n", addr-1, data, i);
        return(1);
    }
    return(0);
}

/* performs mass erase */
int once_flash_mass_erase(flash_constants flash_param) {
    once_move_data_to_r1(flash_param.interface_address);	/* MOVE #<base address>,R1  */
    if (flash_param.interface_address==0x1380) {/* see page 5-18 in the user's manual: Bflash in 807 is an exeption */
        once_move_data_to_r0(0xf800);						/* MOVE #0xF800,R0 			*/
    } else {
        once_move_data_to_r0(flash_param.flash_start);		/* MOVE #<address>,R0 		*/
    }
    once_move_xr1_inc_to_y0();					/* MOVE x:R1,Y0				*/
    once_move_y0_to_xmem(0xffff);				/* MOVE Y0,<OPGDBR> 		*/
    if (once_opgdbr_read()&0x8000) {			/* Read OPGDBR register 	*/
        printf("Flash mass erase failed, BUSY bit is set.\n");
        return(1);
    }
    once_move_data_to_r1(flash_param.interface_address+2); /* MOVE #<base address+2>,R1 */
    if (flash_param.interface_address==0x1380) {/* see page 5-18 in the user's manual: Bflash in 807 is an exeption */
        once_move_data_to_y0(0x4078);			/* MOVE #<ee>,Y0			*/
    } else {
        once_move_data_to_y0(0x4000);			/* MOVE #<ee>,Y0			*/
    }
    once_move_y0_to_xr1_inc();					/* MOVE Y0,x:R1	(FIU_EE)	*/
    once_move_data_to_r1(flash_param.interface_address);	/* MOVE #<base address>,R1	 */
    once_move_data_to_y0(0x0002|(info_block?0x0040:0));		/* MOVE #<cntl>,Y0			 */
    once_move_y0_to_xr1_inc();					/* MOVE Y0,x:R1	(FIU_CNTL)*/
    if (!(flash_param.program_memory)) {
        once_move_y0_to_xr0_inc();				/* MOVE Y0,x:R0	(write to x:addr)*/
    } else {
        once_move_y0_to_pr0_inc();				/* MOVE Y0,x:R0	(write to p:addr)*/
    }
    do {
        once_move_data_to_r1(flash_param.interface_address);	/* MOVE #<base address>,R1  */
        once_nop();								/* NOP						*/
        once_move_xr1_inc_to_y0();				/* MOVE x:R1,Y0				*/
        once_move_y0_to_xmem(0xffff);			/* MOVE Y0,<OPGDBR> 		*/
    }
    while (once_opgdbr_read()&0x8000);	/* repeat poll while BUSY is set */
    once_move_data_to_r1(flash_param.interface_address+2);		/* MOVE #<base address+2>,R1 */
    once_move_data_to_r0(flash_param.interface_address);		/* MOVE #<base address>,R0	 */
    once_move_data_to_y0(info_block?0x0040:0);					/* MOVE #IFREN,Y0			 */
    once_move_y0_to_xr0_inc();					/* MOVE Y0,x:R0	(FIU_CNTL)	*/
    once_move_y0_to_xr1_inc();					/* MOVE Y0,x:R1	(FIU_EE)	*/
    printf("Flash (%#x) mass erase done.\n", flash_param.interface_address);
    return(0);
}

/* performs all page erases needed for programming  */
int once_flash_page_erase(flash_constants flash_param) {
    int page_number,addr,count=0;
    addr=flash_param.start_addr;
    page_number=flash_param.start_addr/256;							/* pages are 256 words long */
    once_move_data_to_r1(flash_param.interface_address);			/* MOVE #<base address>,R1	*/
    once_move_data_to_y0(0x0004|(info_block?0x0040:0));				/* MOVE #<cntl>,Y0			*/
    once_move_y0_to_xr1_inc();										/* MOVE Y0,x:R1	(FIU_CNTL)	*/
    while (page_number*256<=flash_param.flash_end) {
        if ((flash_param.page_erase_map[(addr-flash_param.flash_start)/256]&2)&&(!(flash_param.page_erase_map[(addr-flash_param.flash_start)/256]&1))) {	/* bit 0 says whether the page was already erased, bit 1 says whether to erase the page or not */
            /* now perform page erase of page "page_number" */
            count++;
            flash_param.page_erase_map[(addr-flash_param.flash_start)/256]|=1;		/* mark the page as erased */
            once_move_data_to_r1(flash_param.interface_address);	/* MOVE #<base address>,R1	*/
            once_move_data_to_r0(addr);								/* MOVE #<address>,R0		*/
            once_move_xr1_inc_to_y0();								/* MOVE x:R1,Y0				*/
            once_move_y0_to_xmem(0xffff);							/* MOVE Y0,<OPGDBR>			*/
            if (once_opgdbr_read()&0x8000) {						/* Read OPGDBR register		*/
                printf("Flash page erase failed, BUSY bit is set.\n");
                return(1);
            }
            once_move_data_to_r1(flash_param.interface_address+2);	/* MOVE #<base address+2>,R1 */
            once_move_data_to_y0(0x4000|(page_number&0x007F));		/* MOVE #<ee>,Y0			*/
            once_move_y0_to_xr1_inc();								/* MOVE Y0,x:R1	(FIU_EE)	*/
            if (!(flash_param.program_memory)) {
                once_move_y0_to_xr0_inc();							/* MOVE Y0,x:R0	(write to x:addr) */
            } else {
                once_move_y0_to_pr0_inc();							/* MOVE Y0,p:R0	(write to p:addr) */
            }
            do {
                once_move_data_to_r1(flash_param.interface_address); /* MOVE #<base address>,R1	*/
                once_nop();											/* NOP						*/
                once_move_xr1_inc_to_y0();							/* MOVE x:R1,Y0				*/
                once_move_y0_to_xmem(0xffff);						/* MOVE Y0,<OPGDBR>			*/
            } while (once_opgdbr_read()&0x8000);					/* repeat poll while BUSY is set */
        } else {
            if (flash_param.page_erase_map[(addr-flash_param.flash_start)/256]&3) printf("Page erase of page #%d skipped (flash %#x)\n",page_number,flash_param.interface_address);
        }
        addr+=256;													/* advance to next page */
        page_number++;
    }
    once_move_data_to_r1(flash_param.interface_address+2);		/* MOVE #<base address+2>,R1 */
    once_move_data_to_r0(flash_param.interface_address);		/* MOVE #<base address>,R0	*/
    once_move_data_to_y0(info_block?0x0040:0);					/* MOVE #0,Y0				*/
    once_move_y0_to_xr0_inc();									/* MOVE Y0,x:R0	(FIU_CNTL)	*/
    once_move_y0_to_xr1_inc();									/* MOVE Y0,x:R1	(FIU_EE)	*/
    printf("Flash (%#x) page erase done, %d page(s) erased.\n", flash_param.interface_address,count);
    return(0);
}

/* program flash */
int once_flash_program(flash_constants flash_param) {
    unsigned int i,j;
    unsigned int *data;
    once_init_flash_iface(flash_param);
    if (!page_erase) {
        if (flash_param.duplicate) printf("Mass erase skipped.\n");
        else {
            j = once_flash_mass_erase(flash_param);
            if (j) return(j);
        }
    } else {
        j = once_flash_page_erase(flash_param);
        if (j) return(j);
    }
    j=flash_param.start_addr;
    data=flash_param.data+(flash_param.start_addr-flash_param.flash_start);
    once_flash_program_prepare (flash_param.interface_address, j);
    once_flash_program_pg_no(j);
    for (i=0;i<flash_param.data_count;i++) {
        if (!(j%32)) once_flash_program_pg_no(j);
        if (!(i%512)) printf("p");
        once_flash_program_1word(flash_param, *(data++));
        j++;
    }
    printf("\n");
    data=flash_param.data+(flash_param.start_addr-flash_param.flash_start);
    once_flash_program_end();
    for (i=0;i<flash_param.data_count;i++) {
        if (once_flash_verify_1word(flash_param, *(data++))) return(1);
        if (!(i%512)) printf("v");
    }
    printf("\nFlash (%#x) programming done. %#x words written.\n", flash_param.interface_address, flash_param.data_count);
    return(0);
}

/* prepares flash reading */
/* R2 = start address */
void once_flash_read_prepare (unsigned int addr, flash_constants flash_param[], int flash_count) {
    int i;
    if (info_block) {							/* set IFREN bit of all flash units */
        once_move_data_to_y0(0x0040);			/* MOVE #0x0040,Y0		*/
        for (i=0;i<flash_count;i++) {
            once_move_data_to_r2(flash_param[i].interface_address);	/* MOVE #<base address>,R2 */
            once_move_y0_to_xr2_inc();			/* set IFREN in FIU_CNTL register */
        }
    }
    once_move_data_to_r2(addr);					/* MOVE #<address>,R2 	*/
}

/* read one word */
/* if program_memory!=0, program memory is read */
unsigned int once_flash_read_1word(unsigned char program_memory) {
    if (!(program_memory)) {
        once_move_xr2_inc_to_y0();			/* MOVE x:R2,Y0	(x:addr)	 */
    } else {
        once_move_pr2_inc_to_y0();			/* MOVE p:R2,Y0 (p:addr)	 */
    }
    once_move_y0_to_xmem(0xffff);			/* MOVE Y0,<OPGDBR> 		 */
    return(once_opgdbr_read());				/* Read OPGDBR register 	 */
}

/* read memory */
void once_flash_read(unsigned char program_memory, unsigned int start_addr,
                     unsigned int end_addr, unsigned int *buffer, flash_constants flash_param[], int flash_count) {
    unsigned long int count=end_addr-start_addr+1;
    unsigned int i;
    once_flash_read_prepare (start_addr, flash_param, flash_count);
    for(i=0;i<count;i++) {
        *(buffer++)=once_flash_read_1word(program_memory);
        if (!(i%512)) printf("r");
    }
    printf("\n");
    printf("\nReading memory done, %#lx word(s) read.\n", count);
}





