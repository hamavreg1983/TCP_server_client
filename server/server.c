/*
 * server.c
 *
 *  Created on: Jul 4, 2017
 *      Author: uv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "tcp.h"

/* global for sigaction */
TCP_t* g_object2Destroy = NULL;

void sigAbortHandler(int dummy)
{
	printf("\nFound Sig Cleaning and exit\n\n");
	if (NULL != g_object2Destroy)
	{
		TCP_DestroyServer(g_object2Destroy);
	}

    _exit(2);
}

bool MyFunc(void* _data, size_t _sizeData, void* _contex)
{
	printf("Recive:%s. \n", (char*) _data);
	memcpy(_data, "!", 1);

	return TRUE;
}

int main(int argc, char* argv[])
{
	printf("--START--\n");

	uint portNum = 4848;
	TCP_t* server;

	struct sigaction psa;
	psa.sa_handler = sigAbortHandler;
	sigaction(SIGINT, &psa, NULL);

	server = TCP_CreateServer(portNum);
	g_object2Destroy = server;

	TCP_DoServer(server, MyFunc);

	TCP_DestroyServer(server);
	g_object2Destroy = NULL;
	printf("--END--\n");
}
