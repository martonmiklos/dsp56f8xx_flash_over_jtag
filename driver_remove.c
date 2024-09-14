#include <stdio.h>
#include <windows.h>
#include "port_io.h"

int main(int argc,char *argv[]) {
	driverstop("zlportio");
	if (driverremove("zlportio")) {
		printf("The I/O port driver cannot be removed from the system.\r\n");
	} else {
		printf("The I/O port driver was removed from the system.\r\n");
	}
	return (0);
}
