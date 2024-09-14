/*****************************************************************************
*
* Motorola Inc.
* (c) Copyright 2001,2002 Motorola, Inc.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* File Name:         port_io.c
*
* Description:       Access to I/O ports under Windows NT & 2k
*
* Modules Included:  None
*
* Author: Daniel Malik (daniel.malik@motorola.com)
*
* Comments: Based on ZLPORT driver from Alexander Zloba (zal@specosoft.com)
*
****************************************************************************/

#include <stdio.h>
#include <windows.h>
#include <winsvc.h>
#include <conio.h>
#include "port_io.h"

/* zliostarted - status of the driver, zliodirect-direct access to ports */
char zliostarted=0, zliodirect=0;
/* handle to zlio driver */
HANDLE hzlio;

/* installs a driver */
char driverinstall(const char *path, const char *name) {
	SC_HANDLE hService;
	SC_HANDLE hSCMan;
	hSCMan = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  	if (hSCMan == 0) return(-1);
	hService = CreateService(hSCMan, name, name,
              SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START,
              SERVICE_ERROR_NORMAL, path,
              NULL, NULL, NULL, NULL, NULL);
  	if (hService == 0) {
	  	CloseServiceHandle(hSCMan);
		return(-1);
	}
	CloseServiceHandle(hService);
  	CloseServiceHandle(hSCMan);
	return(0);
}

/* uninstalls a driver */
char driverremove(const char *name) {
	SC_HANDLE hService;
	SC_HANDLE hSCMan;
	hSCMan = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  	if (hSCMan == 0) return(-1);
  	// get a handle to the service
  	hService = OpenService(hSCMan, name, SERVICE_ALL_ACCESS);
  	if (hService!=0) {
    	char result=0;
		// remove driver description from the registry
    	if (!DeleteService(hService)) result=-1;
    	CloseServiceHandle(hService);
    	CloseServiceHandle(hSCMan);
		return(result);
	}
	CloseServiceHandle(hSCMan);
	return(-1);
}

/* start a driver */
char driverstart(const char *name) {
	SC_HANDLE hService;
	SC_HANDLE hSCMan;
	hSCMan = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
  	if (hSCMan == 0) return(-1);
	// get a handle to the service
  	hService = OpenService(hSCMan, name, SERVICE_START);
  	if (hService!=0) {
    	char result=0;
    	// start the driver
		if (!StartService(hService, 0, NULL)) result=-1;
	    CloseServiceHandle(hService);
		CloseServiceHandle(hSCMan);
		return(result);
	}
	CloseServiceHandle(hSCMan);
	return(-1);
}

/* stop a driver */
char driverstop(const char *name) {
	SC_HANDLE hService;
	SC_HANDLE hSCMan;
	SERVICE_STATUS serviceStatus;

	hSCMan = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
  	if (hSCMan == 0) return(-1);
	// get a handle to the service
  	hService = OpenService(hSCMan, name, SERVICE_STOP);
  	if (hService!=0) {
    	char result=0;
    	// start the driver
		if (!ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus)) result=-1;
	    CloseServiceHandle(hService);
		CloseServiceHandle(hSCMan);
		return(result);
	}
	CloseServiceHandle(hSCMan);
	return(-1);
}

/* 1=win NT platform, 0=other windows platform */
char win_nt(void) {
	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx (&info);
	if (info.dwPlatformId==VER_PLATFORM_WIN32_NT) return(1);
	return(0);
}

/* displays last error message - for debugging purposes only */
void DisplayLastError(void) {
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), 0, (LPTSTR) &lpMsgBuf, 0, NULL);
	mesg("%s",lpMsgBuf);
}

/* opens the zlio driver */
char zlioopen(HANDLE *handle) {
	if (!win_nt()) return(0);
	*handle=CreateFile("\\\\.\\zlportio", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (*handle == INVALID_HANDLE_VALUE) {
		return(-1);
	}
	return(0);
}

/* stops the zlio driver */
void zliostop(void) {
	if (!win_nt()) return;
 	if (!zliostarted) return;
	CloseHandle(hzlio);
	driverstop("zlportio");
	zliostarted=0;
	zliodirect=0;
//	driverremove("zlportio");
}

/* starts the zlio driver */
char zliostart(void) {
	FILE *sys_file;
	driverstop("zlportio");			/* stop the driver in case it is already started */
	if (driverstart("zlportio")) {	/* sty to start it */
		char path[PATH_LENGTH];
		driverstop("zlportio");		/* not installed ? */
		mesg("I/O port driver not installed\r\n");
		mesg("Installing...\r\n");
		GetModuleFileName(NULL,path,sizeof(path));
		*strrchr(path,'\\')='\0';
		strcat(path,"\\zlportio.sys");
		sys_file=fopen(path,"r");
		if (sys_file==NULL) {
			mesg("Cannot find the I/O port driver \r\n");
			mesg("Make sure the I/O port driver is present in the same directory as the exe file\r\n");
			return(-1);
		}
		fclose(sys_file);
		if (driverinstall(path,"zlportio")) {	/* try to install the driver */
			mesg("Make sure the I/O port driver is present in the same directory as the exe file\r\n");
			return(-1);
		}
		if (driverstart("zlportio")) {			/* start the driver */
			mesg("I/O port driver cannot be started\r\n");
			return(-1);
		}
	}
	mesg("I/O port driver started\r\n");	/* success, driver started */
	return(0);

//	char path[200];
//	if (!win_nt()) return(0);
//	zliostop();
//	GetCurrentDirectory(sizeof(path),path);
//	strcat(path,"\\zlportio.sys");
//	driverinstall(path,"zlportio");
//	return(driverstart("zlportio"));
}

/* initialises the driver */
char zlioinit(void) {
	if (!win_nt()) {
		zliostarted=1;
		zliodirect=1;
	} else {
		if (zlioopen(&hzlio)) {
			if (!zliostart()) zliostarted=(!zlioopen(&hzlio)) || (!win_nt());
		} else zliostarted=1;
	}
	return(zliostarted);
}

/* port read */
DWORD zlioportread(DWORD port, DWORD datatype) {
	tzliodata resdata;
	DWORD i,cbr;
	if (!zliodirect) {
		resdata.port = port;
		resdata.datatype = datatype;
		if (zliostarted)
			DeviceIoControl(hzlio,IOCTL_ZLUNI_PORT_READ,&resdata,sizeof(resdata),&i,sizeof(DWORD),&cbr,NULL);
	} else {
		switch (datatype) {
			case ZLIO_BYTE:		return(_inp(port));
			case ZLIO_WORD:		return(_inpw(port));
			case ZLIO_DWORD:	return(_inpd(port));
		}
	}
	return(i);
}

/* port write */
void zlioportwrite(DWORD port, DWORD datatype, DWORD data) {
	tzliodata resdata;
	DWORD cbr;
	if (!zliodirect) {
		resdata.port = port;
		resdata.datatype = datatype;
		resdata.data = data;
		if (zliostarted) {
			DeviceIoControl(hzlio,IOCTL_ZLUNI_PORT_WRITE,&resdata,sizeof(resdata),NULL,0,&cbr,NULL);
		}
	} else {
		switch (datatype) {
			case ZLIO_BYTE:		_outp(port,data); return;
			case ZLIO_WORD:		_outpw(port,data); return;
			case ZLIO_DWORD:	_outpd(port,data); return;
		}
	}
}

/* enable/disable direct port access */
char zliosetiopm(char direct_access) {
	DWORD cbr;
	if (win_nt()&&zliostarted) {
		if (direct_access) {
			DeviceIoControl(hzlio,IOCTL_ZLUNI_IOPM_ON,NULL,0,NULL,0,&cbr,NULL);
		} else {
			DeviceIoControl(hzlio,IOCTL_ZLUNI_IOPM_OFF,NULL,0,NULL,0,&cbr,NULL);
		}
		zliodirect=direct_access;
	}
	return(zliodirect);
}
