/*****************************************************************************
*
* Motorola Inc.
* (c) Copyright 2001,2002 Motorola, Inc.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* File Name:         port_io.h
*
* Description:       prototypes of functions for gaining access to I/O ports under Windows NT & 2k
*
* Modules Included:  None
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
* Comments: Based on ZLPORT driver from Alexander Zloba (zal@specosoft.com)
*
****************************************************************************/

#define FILE_DEVICE_KRNLDRVR	0x80ff
#define METHOD_BUFFERED			0
#define FILE_ANY_ACCESS			0

#define IOCTL_ZLUNI_PORT_READ	((FILE_DEVICE_KRNLDRVR<<16)|(FILE_ANY_ACCESS<<14)|(0x1<<2)|(METHOD_BUFFERED))
#define IOCTL_ZLUNI_PORT_WRITE	((FILE_DEVICE_KRNLDRVR<<16)|(FILE_ANY_ACCESS<<14)|(0x2<<2)|(METHOD_BUFFERED))
#define IOCTL_ZLUNI_IOPM_ON		((FILE_DEVICE_KRNLDRVR<<16)|(FILE_ANY_ACCESS<<14)|(0x3<<2)|(METHOD_BUFFERED))
#define IOCTL_ZLUNI_IOPM_OFF	((FILE_DEVICE_KRNLDRVR<<16)|(FILE_ANY_ACCESS<<14)|(0x4<<2)|(METHOD_BUFFERED))

#define ZLIO_BYTE	0
#define ZLIO_WORD	1
#define ZLIO_DWORD	2

typedef struct {
	DWORD	port;
	DWORD	datatype;
	DWORD	data;
} tzliodata;

char win_nt(void);		/* 1=win NT platform, 0=other windows platform */
char zlioinit(void);	/* initialises the driver */
void zliostop(void);	/* stops the driver */
DWORD zlioportread(DWORD port, DWORD datatype);				/* port read */
void zlioportwrite(DWORD port, DWORD datatype, DWORD data);	/* port write */
char zliosetiopm(char direct_access);						/* enable/disable direct port access */
char driverstop(const char *name);		/* stop a driver */
char driverremove(const char *name);	/* uninstalls a driver */

/* message printing */
#define	mesg	printf

/* maximum path lenght */
#define PATH_LENGTH	255
