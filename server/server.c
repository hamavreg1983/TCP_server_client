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

typedef void (*sigHandler)(int sig, siginfo_t *siginfo, void *context);
void sigAbortHandler(int sig, siginfo_t *siginfo, void *context)
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

	struct sigaction act;
	memset (&act, '\0', sizeof(act));

	/* Use the sa_sigaction field because the handles has two additional parameters */
	act.sa_sigaction = _func;
	/* The SA_SIGINFO flag tells sigaction() to use the sa_sigaction field, not sa_handler. */
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGINT, &act, NULL) < 0) {
		perror ("SIGINT");
		return 1;
	}

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
	uint timeoutMS = 300000; /* 5 min */

	/* TODO option get ip and port from agrc */

	signalHangelSet(sigAbortHandler);

	server = TCP_CreateServer(portNum, NULL, MAX_CONNECTIONS_ALLWAED, timeoutMS, MyFunc, NULL, NULL, NULL, NULL);
	g_tcp = server;

	TCP_RunServer(server);

	g_tcp = NULL;
	TCP_DestroyServer(server);
	printf("--END--\n");
}
