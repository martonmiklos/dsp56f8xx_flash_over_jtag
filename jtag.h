/*****************************************************************************
*
* Motorola Inc.
* (c) Copyright 2001,2002 Motorola, Inc.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* File Name:         jtag.h
*
* Description:       Jtag and OnCE access prototypes and macros
*
* Modules Included:  None
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
****************************************************************************/
#ifndef JTAG____H
#define JTAG____H
#include <stdint.h>

#include "flash.h"

#define RETRY_DEBUG	10			/* how many JTAGIR polls should we try to wait for entry into DEBUG mode */
#define JTAG_PATH_LEN_MAX 256	/* maximum JTAG DR & IR path lenght. High numbers do not matter, but the measure routine will take longer to execute */

/* prototypes */

int jtag_init(void);	/* returns 0 on success, -1 if command converter not found */
int jtag_instruction_exec(int instruction);
unsigned long int jtag_data_shift(unsigned long int data, int bit_count);
int init_target (void);
int once_init_flash_iface(flash_constants flash_param);
void once_flash_program_prepare (unsigned int fiu_address, unsigned int addr);
void once_flash_program_pg_no (unsigned int addr);
void once_flash_program_end (void);
void once_flash_program_1word(flash_constants flash_param, unsigned int data);
int once_flash_verify_1word(flash_constants flash_param, unsigned int data);
int once_flash_mass_erase(flash_constants flash_param);
int once_flash_page_erase(flash_constants flash_param);
int once_flash_program(flash_constants flash_param);
void jtag_disconnect(void);
void jtag_data_write8(unsigned int data);
void jtag_data_write16(unsigned int data);
unsigned int jtag_data_read16(void);
void set_port(unsigned int port);
void set_info_block(unsigned int value);

/* exit mode - leave the part in debug mode or reset it */
void set_exit_mode(unsigned char mode);

/* reading memory */
void once_flash_read_prepare (unsigned int addr, flash_constants flash_param[], int flash_count);
unsigned int once_flash_read_1word(unsigned char program_memory);
void once_flash_read(unsigned char program_memory, unsigned int start_addr, unsigned int end_addr, unsigned int *buffer, flash_constants flash_param[], int flash_count);

/* erase mode */
void set_erase_mode(unsigned char mode);

/* routines for handling multiple devices in the JTAG chain */
int jtag_measure_paths(void);
int get_data_pl(void);		/* returns data path lenght measured by the measure routine */
int get_instr_pl(void);		/* returns instruction path lenght measured by the measure routine */
int get_data_pp(void);
int get_instr_pp(void);
void set_data_pp(int length);
void set_instr_pp(int length);

/* wait for DSP to come out of reset */
void set_DSP_wait(char wait);

/* Executes OnCE command */
#define once_instruction_exec(instruction, rw, go, ex) jtag_data_write8(instruction|(ex<<5)|(go<<6)|(rw<<7))

/* Transfers 16-bit data into the OnCE through DR-shift of Jtag */
#define once_data_write(data) jtag_data_write16(data)

/* Reads 16-bit data from the OnCE through DR-shift of Jtag */
#define once_data_read() jtag_data_read16()

/* Executes one word DSP instruction */
#define once_execute_instruction1(opcode) once_instruction_exec(0x09,0,1,0);\
once_data_write(opcode)

/* Executes two word DSP instruction */
#define once_execute_instruction2(opcode1, opcode2) once_instruction_exec(0x09,0,0,0);\
    once_data_write(opcode1);\
    once_instruction_exec(0x09,0,1,0);\
    once_data_write(opcode2)

/* Executes two word DSP instruction and exits the debug mode */
#define once_execute_instruction1_run(opcode) once_instruction_exec(0x09,0,1,1);\
    once_data_write(opcode);

/* Reads contents of the OPGDBR register */
#define once_opgdbr_read()	(once_instruction_exec(0x08,1,1,0), once_data_read())

/* Reads contents of the OPDBR register */
#define once_opdbr_read()	(once_instruction_exec(0x09,1,1,0), once_data_read())


/* --------------- Instructions --------------- */

/* MOVE <data>,Y0 */
#define once_move_data_to_y0(data) once_execute_instruction2(0x87c1,data)

/* MOVE <data>,Y1 */
#define once_move_data_to_y1(data) once_execute_instruction2(0x87c3,data)

/* MOVE Y0,x:address */
#define once_move_y0_to_xmem(address) once_execute_instruction2(0xd154,address)

/* MOVE x:address,Y0 */
#define once_move_xmem_to_y0(address) once_execute_instruction2(0xf154,address)

/* NOP */
#define once_nop() once_execute_instruction1(0xe040)

/* MOVE <data>,R0 */
#define once_move_data_to_r0(data) once_execute_instruction2(0x87d0,data)

/* MOVE <data>,R1 */
#define once_move_data_to_r1(data) once_execute_instruction2(0x87d1,data)

/* MOVE <data>,R2 */
#define once_move_data_to_r2(data) once_execute_instruction2(0x87d2,data)

/* MOVE <data>,R3 */
#define once_move_data_to_r3(data) once_execute_instruction2(0x87d3,data)

/* MOVE <data>,x:(R2+<offset>) */ /* caution, the offset only 6 bits plus sign ! */
// does not work, do not ask me why... #define once_move_data_to_xr2_off(data,offset) once_execute_instruction2(0xA680|(offset&0x007f),data)

/* MOVE x:(R0),Y0 */
#define once_move_xr0_to_y0() once_execute_instruction1(0xf114)

/* MOVE x:(R1),Y0 */
#define once_move_xr1_to_y0() once_execute_instruction1(0xf115)

/* MOVE x:(R2),Y0 */
#define once_move_xr2_to_y0() once_execute_instruction1(0xf116)

/* MOVE x:(R3),Y0 */
#define once_move_xr3_to_y0() once_execute_instruction1(0xf117)

/* MOVE Y0,x:(R1) */
#define once_move_y0_to_xr1() once_execute_instruction1(0xd115)

/* MOVE x:(R0)+,Y0 */
#define once_move_xr0_inc_to_y0() once_execute_instruction1(0xf100)

/* MOVE x:(R1)+,Y0 */
#define once_move_xr1_inc_to_y0() once_execute_instruction1(0xf101)

/* MOVE x:(R2)+,Y0 */
#define once_move_xr2_inc_to_y0() once_execute_instruction1(0xf102)

/* MOVE x:(R3)+,Y0 */
#define once_move_xr3_inc_to_y0() once_execute_instruction1(0xf103)

/* MOVE Y0,x:(R0)+ */
#define once_move_y0_to_xr0_inc() once_execute_instruction1(0xd100)

/* MOVE Y0,x:(R1)+ */
#define once_move_y0_to_xr1_inc() once_execute_instruction1(0xd101)

/* MOVE Y0,x:(R2)+ */
#define once_move_y0_to_xr2_inc() once_execute_instruction1(0xd102)

/* MOVE Y0,x:(R3)+ */
#define once_move_y0_to_xr3_inc() once_execute_instruction1(0xd103)

/* MOVE Y1,x:(R0)+ */
#define once_move_y1_to_xr0_inc() once_execute_instruction1(0xd300)

/* MOVE R0,Y0 */
#define once_move_r0_to_y0() once_execute_instruction1(0x8110)

/* MOVE R1,Y0 */
#define once_move_r1_to_y0() once_execute_instruction1(0x8111)

/* MOVE R2,Y0 */
#define once_move_r2_to_y0() once_execute_instruction1(0x8112)

/* MOVE R3,Y0 */
#define once_move_r3_to_y0() once_execute_instruction1(0x8113)

/* MOVE OMR,Y0 */
#define once_move_omr_to_y0() once_execute_instruction1(0x8118)

/* MOVE Y0,OMR */
#define once_move_y0_to_omr() once_execute_instruction1(0x8881)

/* MOVE Y0,p:(R0)+ */
#define once_move_y0_to_pr0_inc() once_execute_instruction1(0xe100)

/* MOVE Y0,p:(R1)+ */
#define once_move_y0_to_pr1_inc() once_execute_instruction1(0xe101)

/* MOVE Y0,p:(R2)+ */
#define once_move_y0_to_pr2_inc() once_execute_instruction1(0xe102)

/* MOVE Y0,p:(R3)+ */
#define once_move_y0_to_pr3_inc() once_execute_instruction1(0xe103)

/* MOVE Y1,p:(R0)+ */
#define once_move_y1_to_pr0_inc() once_execute_instruction1(0xe300)

/* MOVE p:(R0)+,Y0 */
#define once_move_pr0_inc_to_y0() once_execute_instruction1(0xe120)

/* MOVE p:(R1)+,Y0 */
#define once_move_pr1_inc_to_y0() once_execute_instruction1(0xe121)

/* MOVE p:(R2)+,Y0 */
#define once_move_pr2_inc_to_y0() once_execute_instruction1(0xe122)

/* MOVE p:(R3)+,Y0 */
#define once_move_pr3_inc_to_y0() once_execute_instruction1(0xe123)

/* MOVE sp,y0 */
#define once_move_sr_to_y0() once_execute_instruction1(0x811d)

/* MOVE y0,sr */
#define once_move_y0_to_sr() once_execute_instruction1(0x8d81)

/* JMP addr */
#define once_jmp(addr) once_execute_instruction2(0xE984,addr)

/* JMP addr + exit debug mode: execute JMP addr and NOP with exit from debug mode */
#define once_jmp_run(addr) 	once_execute_instruction2(0xE984,addr);\
once_execute_instruction1_run(0xe040)
#endif
