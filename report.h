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
* Description:       Prototypes for printing and logging
*
* Modules Included:  None
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
****************************************************************************/

int print(const char *format, ...);
int open_log(char *path);
void close_log(void);

