/*
 * tcp_client.c
 *
 *  Created on: Jul 4, 2017
 *      Author: Yuval Hamberg
 */

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "list.h"
#include "tcp.h"
#include "tcp_client.h"

/* ~~~ Defines ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#define ALIVE_MAGIC_NUMBER	0xfadeface
#define DEAD_MAGIC_NUMBER	0xdeadface

#define GENERAL_ERROR -9
#define BACK_LOG_CAPACITY 128

/* ~~~ Global ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~ Struct ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

struct TCP_S
{
	int m_magicNumber;

	int m_connectionCapacity;
	int m_connectedNum; /* count the amount of open connections */
	int m_listenSocket;
	list_t* m_sockets; /* list of sockets */

	int m_commSocket; /* used for client */

	uint m_serverPort;
	char m_serverIP[INET6_ADDRSTRLEN];

	bool m_isServerRun;

	userActionFunc m_reciveDataFunc;
	clientConnectionChangeFunc m_newConnectionFunc;
	clientConnectionChangeFunc m_closedConnectionFunc;
	errorFunc m_errorFunc;

};

struct TCP_C
{
	int m_magicNumber;

	int m_commSocket; /* used for client */

	uint m_serverPort;
	char m_serverIP[INET6_ADDRSTRLEN];

};

/* ~~~ Internal function forward declaration ~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief If the client need to know the server socket it is connected to.
 * @param _TCP the pointer to the client struct
 * @return int that represent the file descriptor (socket) of this client
 */
int TCP_ClientGetSocket(TCP_S_t* _TCP);

static bool IsStructValid(TCP_S_t* _TCP);
static bool IsConnected(TCP_S_t* _TCP);

bool TCP_ClientConnect(TCP_S_t* _TCP);


/* ~~~ API function ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


TCP_S_t* TCP_CreateClient(char* _ServerIP, uint _serverPort)
{
	TCP_S_t* aTCP = 0;
	aTCP = malloc(1 * sizeof(TCP_S_t) );
	if (!aTCP)
	{
		/* ERROR */
		return NULL;
	}

	strncpy(aTCP->m_serverIP , _ServerIP, INET6_ADDRSTRLEN);
	aTCP->m_serverPort = _serverPort;
	aTCP->m_connectedNum = 0;
	aTCP->m_sockets = NULL;

	aTCP->m_commSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (aTCP->m_commSocket < 0)
	{
		perror("CreateClient, Socket create Failed");
		free(aTCP);
		return NULL;
	}
	aTCP->m_magicNumber = ALIVE_MAGIC_NUMBER;

	if (! TCP_ClientConnect(aTCP) )
	{
		perror("Socket Connect Failed");
		free(aTCP);
		return NULL;
	}

	return aTCP;
}


void TCP_DestroyClient(TCP_S_t* _TCP)
{
	if ( !IsStructValid(_TCP) )
	{
		return;
	}

	_TCP->m_magicNumber = DEAD_MAGIC_NUMBER;

	close(_TCP->m_commSocket);

	free(_TCP);
	return;
}



bool TCP_ClientConnect(TCP_S_t* _TCP)
{
	struct sockaddr_in client_sIn;
	memset(&client_sIn , 0 , sizeof(client_sIn) );

	client_sIn.sin_family = AF_INET;
	client_sIn.sin_addr.s_addr = inet_addr(_TCP->m_serverIP);
	client_sIn.sin_port = htons(_TCP->m_serverPort);

	if (connect(_TCP->m_commSocket, (struct sockaddr *) &client_sIn, sizeof(client_sIn)) < 0)
	{
		perror("Socket client connect Failed");
		return FALSE;
	}
	_TCP->m_connectedNum++;

	return TRUE;
}



int TCP_ClientGetSocket(TCP_S_t* _TCP)
{
	if ( !IsStructValid(_TCP) || ! IsConnected(_TCP))
	{
		return GENERAL_ERROR;
	}

	return _TCP->m_commSocket;
}

int TCP_ClientSend(TCP_S_t* _TCP, void* _msg, uint _msgLength)
{
	return TCP_Send(_TCP, TCP_ClientGetSocket(_TCP), _msg, _msgLength);
}

int TCP_ClientRecive(TCP_S_t* _TCP, void* _buffer, uint _bufferMaxLength)
{
	return TCP_Recive(_TCP, TCP_ClientGetSocket(_TCP), _buffer, _bufferMaxLength);
}


/* ~~~ Internal function  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool IsStructValid(TCP_S_t* _TCP)
{
	return !(NULL == _TCP || ALIVE_MAGIC_NUMBER != _TCP->m_magicNumber);
}

bool IsConnected(TCP_S_t* _TCP)
{
	return (bool) _TCP->m_connectedNum;
}




