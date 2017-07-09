/*
 * tcp_client.c
 *
 *  Created on: Jul 4, 2017
 *      Author: Yuval Hamberg
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h> /* ADDRSELEN */
/* #include <errno.h> */
#include <unistd.h> /* close */

#include "list.h"
#include "tcp_client.h"

/* ~~~ Defines ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#define ALIVE_MAGIC_NUMBER	0xfadeface
#define DEAD_MAGIC_NUMBER	0xdeadface

#define GENERAL_ERROR -9
#define BACK_LOG_CAPACITY 128

/* ~~~ Global ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~ Struct ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

struct TCP_C
{
	int m_magicNumber;

	int m_commSocket; /* used for client */
	int m_connectedNum; /* count the amount of open connections */

	uint m_serverPort;
	char m_serverIP[INET6_ADDRSTRLEN];

};

/* ~~~ Internal function forward declaration ~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief If the client need to know the server socket it is connected to.
 * @param _TCP the pointer to the client struct
 * @return int that represent the file descriptor (socket) of this client
 */
int TCP_ClientGetSocket(TCP_C_t* _TCP);

static bool IsStructValid(TCP_C_t* _TCP);
static bool IsConnected(TCP_C_t* _TCP);

bool TCP_ClientConnect(TCP_C_t* _TCP);

static void sanity_check(char* _string, uint _size, char _replaceWith);


/* ~~~ API function ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


TCP_C_t* TCP_CreateClient(char* _ServerIP, uint _serverPort)
{
	TCP_C_t* aTCP = 0;
	aTCP = malloc(1 * sizeof(TCP_C_t) );
	if (!aTCP)
	{
		/* ERROR */
		return NULL;
	}

	strncpy(aTCP->m_serverIP , _ServerIP, INET6_ADDRSTRLEN);
	aTCP->m_serverPort = _serverPort;
	aTCP->m_connectedNum = 0;

	aTCP->m_commSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (aTCP->m_commSocket < 0)
	{
		perror("CreateClient, Socket create Failed");
		free(aTCP);
		return NULL;
	}

	/* Reusing port */
	int optval = 1;
	if ( setsockopt(aTCP->m_commSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval) ) < 0)
	{
		perror("Socket setsockopt Failed");
		close(aTCP->m_commSocket);
		free(aTCP);
		return FALSE;
	}

	aTCP->m_magicNumber = ALIVE_MAGIC_NUMBER;

	if (! TCP_ClientConnect(aTCP) )
	{
		perror("Socket Connect Failed");
		close(aTCP->m_commSocket);
		free(aTCP);
		return NULL;
	}

	return aTCP;
}


void TCP_DestroyClient(TCP_C_t* _TCP)
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



bool TCP_ClientConnect(TCP_C_t* _TCP)
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



int TCP_ClientGetSocket(TCP_C_t* _TCP)
{
	if ( !IsStructValid(_TCP) || ! IsConnected(_TCP))
	{
		return GENERAL_ERROR;
	}

	return _TCP->m_commSocket;
}

int TCP_ClientSend(TCP_C_t* _TCP, void* _msg, uint _msgLength)
{
	if ( !IsStructValid(_TCP) || ! IsConnected(_TCP))
	{
		return GENERAL_ERROR;
	}
	if ( NULL == _msg)
	{
		return GENERAL_ERROR;
	}

	int sent_bytes;
	sent_bytes = send( _TCP->m_commSocket, _msg, _msgLength, 0 );

	if (0 > sent_bytes)
	{
		perror("Send Failed");
	}

	return sent_bytes;
}

int TCP_ClientRecive(TCP_C_t* _TCP, void* _buffer, uint _bufferMaxLength)
{
	int nBytesRead;

	if ( !IsStructValid(_TCP) || ! IsConnected(_TCP))
	{
		return GENERAL_ERROR;
	}
	if ( NULL == _buffer)
	{
		return GENERAL_ERROR;
	}

    nBytesRead = recv( _TCP->m_commSocket, _buffer, _bufferMaxLength , 0 );

    if (nBytesRead == 0)
    {
		#if !defined(NDEBUG) /* DEBUG */
    	printf("socket #%d was closed on server side. should disconnect from server.\n", _TCP->m_commSocket);
		#endif
    }

    sanity_check(_buffer, _bufferMaxLength, '_');

    return nBytesRead;
}


/* ~~~ Internal function  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool IsStructValid(TCP_C_t* _TCP)
{
	return !(NULL == _TCP || ALIVE_MAGIC_NUMBER != _TCP->m_magicNumber);
}

bool IsConnected(TCP_C_t* _TCP)
{
	return (bool) _TCP->m_connectedNum;
}

static void sanity_check(char* _string, uint _size, char _replaceWith)
{
    int j = 0;

    while (j < _size)
    {
    	if ( 	(_string[j] <= 'z' && _string[j] >= 'a')
			|| 	(_string[j] <= 'Z' && _string[j] >= 'A' )
			||	(_string[j] <= '9' && _string[j] >= '0' )
			|| 	_string[j] == ' '
			|| 	_string[j] == '?'
			|| 	_string[j] == '!'
			|| 	_string[j] == '"'
			|| 	_string[j] == '.'
			|| 	_string[j] == ','
			|| 	_string[j] == ':'
			|| 	_string[j] == '\''
			|| 	_string[j] == '\0'
    		)
    	{
    		/* good char, do nothing */
    	}
    	else
    	{
    		_string[j] = _replaceWith ;
        }
        j++;
    }
    return;
}



