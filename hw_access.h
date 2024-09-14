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

#define JTAG_PORT_DEFAULT	0
#define JTAG_RESET_MASK		0x0001		/* D0, pin 2 */
#define JTAG_TMS_MASK		0x0002		/* D1, pin 3 */
#define JTAG_TCK_MASK		0x0004		/* D2, pin 4 */
#define JTAG_TDI_MASK		0x0008		/* D3, pin 5 */
#define JTAG_TRST_MASK		0x0010		/* D4, pin 6 */
#define JTAG_TDO_MASK		0x0020		/* BUSY, pin 11 */
#define JTAG_TDO_SHIFT		7			/* number of bits to shift the data register to get the TDO value at bit 0 */

/**************************************************************************/

#define JTAG_TCK_SET		pport_data|=JTAG_TCK_MASK;outp(jtag_port,pport_data)
#define JTAG_TCK_RESET		pport_data&=~JTAG_TCK_MASK;outp(jtag_port,pport_data)
/*
#define JTAG_TMS_SET		pport_data|=JTAG_TMS_MASK;outp(jtag_port,pport_data)
#define JTAG_TMS_RESET		pport_data&=~JTAG_TMS_MASK;outp(jtag_port,pport_data)

#define JTAG_TDI_SET		{pport_data|=JTAG_TDI_MASK;outp(jtag_port,pport_data);}
#define JTAG_TDI_RESET		{pport_data&=~JTAG_TDI_MASK;outp(jtag_port,pport_data);}
*/
#define JTAG_TMS_SET		pport_data|=JTAG_TMS_MASK
#define JTAG_TMS_RESET		pport_data&=~JTAG_TMS_MASK

#define JTAG_TDI_SET		{pport_data|=JTAG_TDI_MASK;}
#define JTAG_TDI_RESET		{pport_data&=~JTAG_TDI_MASK;}

#define JTAG_TDI_ASSIGN(i)	if (i&0x0001) JTAG_TDI_SET else JTAG_TDI_RESET

#define JTAG_TRST_SET		pport_data|=JTAG_TRST_MASK;outp(jtag_port,pport_data)
#define JTAG_TRST_RESET		pport_data&=~JTAG_TRST_MASK;outp(jtag_port,pport_data)

#define JTAG_RESET_SET		pport_data&=~JTAG_RESET_MASK;outp(jtag_port,pport_data)
#define JTAG_RESET_RESET	pport_data|=JTAG_RESET_MASK;outp(jtag_port,pport_data)

#define JTAG_TDO_VALUE				((~inp(jtag_port+1)&JTAG_TDO_MASK) >> JTAG_TDO_SHIFT)
#define JTAG_TDO_VALUE_SHIFTED_15 	((~inp(jtag_port+1)&JTAG_TDO_MASK) << (15-JTAG_TDO_SHIFT))

/*
#define JTAG_TDO_VALUE				(outp(jtag_port,pport_data),((~inp(jtag_port+1)&JTAG_TDO_MASK) >> JTAG_TDO_SHIFT))
#define JTAG_TDO_VALUE_SHIFTED_15 	(outp(jtag_port,pport_data),((~inp(jtag_port+1)&JTAG_TDO_MASK) << (15-JTAG_TDO_SHIFT)))
*/

// extern "C" unsigned int initdelay(void);
// extern "C" void delay50ns(void);

#define INIT_WAIT_FUNCTION
#define WAIT_100_NS			outp(jtag_port,pport_data)

#endif
