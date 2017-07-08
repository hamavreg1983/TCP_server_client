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
TCP_S_t* g_tcp = NULL;

#define MAX_CONNECTIONS_ALLWAED 1000

typedef void (*sigHandler)(int);
void sigAbortHandler(int dummy)
{
	const char notify[] = "\nGot Signal, lets Clean and exit server\n\n";
	write(STDERR_FILENO, notify, strlen(notify));

	TCP_StopServer(g_tcp);

	return;
}

bool signalHangelSet(sigHandler _func)
{
	if (NULL == _func)
	{
		return FALSE;
	}
	/* this way, the sigaction, it is not possible to transfer a pramater to the function. just use a global value */
	struct sigaction psa;
	psa.sa_handler = _func;
	sigaction(SIGINT, &psa, NULL);

	return TRUE;
}

int MyFunc(void* _data, size_t _sizeData, uint _socketNum, void* _contex)
{
	printf("Recive:%s. \n", (char*) _data);
	memcpy(_data, "!", 1);

	if ( TCP_Send(_socketNum, _data, _sizeData) <= 0)
	{
		perror("Send response from server failed.\n");
		return FALSE;
	}

	return TRUE;
}

int main(int argc, char* argv[])
{
	printf("--START--\n");

	uint portNum = 4848;
	TCP_S_t* server;

	signalHangelSet(sigAbortHandler);

	server = TCP_CreateServer(portNum, NULL, MAX_CONNECTIONS_ALLWAED, MyFunc, NULL, NULL, NULL);
	g_tcp = server;

	TCP_RunServer(server);

	g_tcp = NULL;
	TCP_DestroyServer(server);
	printf("--END--\n");
}
