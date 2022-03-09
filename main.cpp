//============================================================================
// Name        : UPNPPortForward.cpp
// Author      : Ryan
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include "UPNPNAT.h"
int main(void) {
	puts("!!!Hello World!!!");
	UPNPNAT nat;
	nat.init(5,10);

	if(!nat.discovery()){
		printf("discovery error is %s\n",nat.get_last_error());
		return -1;
	}


	if(!nat.add_port_mapping("test","192.168.0.101",1236,1234,"TCP")){
		printf("add_port_mapping error is %s\n",nat.get_last_error());
		return -1;
	}

	printf("add port mapping success.\n");
	return EXIT_SUCCESS;
}
