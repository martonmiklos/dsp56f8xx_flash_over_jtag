/*****************************************************************************
*
* Motorola Inc.
* (c) Copyright 2001,2002 Motorola, Inc.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* File Name:         hw_access.h
*
* Description:       Macros for accessing the JTAG hardware
*                    
* Modules Included:  None 
*
* Author: Daniel Malik (daniel.malik@motorola.com)                    
* 
****************************************************************************/

#ifndef HW_ACCESS___H
#define HW_ACCESS___H

#define JTAG_RESET_MASK		0x0001
#define JTAG_TMS_MASK		0x0002
#define JTAG_TCK_MASK		0x0004
#define JTAG_TDI_MASK		0x0008
#define JTAG_TRST_MASK		0x0010
#define JTAG_TDO_MASK		0x0020

/**************************************************************************/

#define JTAG_TCK_SET		pport_data|=JTAG_TCK_MASK;jtag_outp(pport_data)
#define JTAG_TCK_RESET		pport_data&=~JTAG_TCK_MASK;jtag_outp(pport_data)

#define JTAG_TMS_SET		pport_data|=JTAG_TMS_MASK
#define JTAG_TMS_RESET		pport_data&=~JTAG_TMS_MASK

#define JTAG_TDI_SET		{pport_data|=JTAG_TDI_MASK;}
#define JTAG_TDI_RESET		{pport_data&=~JTAG_TDI_MASK;}

#define JTAG_TDI_ASSIGN(i)	if (i&0x0001) JTAG_TDI_SET else JTAG_TDI_RESET

#define JTAG_TRST_SET		pport_data|=JTAG_TRST_MASK;jtag_outp(pport_data)
#define JTAG_TRST_RESET		pport_data&=~JTAG_TRST_MASK;jtag_outp(pport_data)

#define JTAG_RESET_SET		pport_data&=~JTAG_RESET_MASK;jtag_outp(pport_data)
#define JTAG_RESET_RESET	pport_data|=JTAG_RESET_MASK;jtag_outp(pport_data)

#define JTAG_TDO_VALUE				((jtag_inp() & JTAG_TDO_MASK) ? 1 : 0)

// extern "C" unsigned int initdelay(void);
// extern "C" void delay50ns(void);

#define INIT_WAIT_FUNCTION
#define WAIT_100_NS			jtag_outp(pport_data)

#endif

