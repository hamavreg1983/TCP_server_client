/*
 * tcp.c
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

/* ~~~ Defines ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#define ALIVE_MAGIC_NUMBER	0xfadeface
#define DEAD_MAGIC_NUMBER	0xdeadface

#define GENERAL_ERROR -1
#define BACK_LOG_CAPACITY 512
#define BUFFER_MAX_SIZE 1024

#define MAX_CLIENTS_NUM 1000


/* ~~~ Struct ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

struct TCP
{
	int m_magicNumber;

	int m_connectionCapacity;
	int m_connectedNum; /* count the amount of open connections */
	int m_listenSocket;
	list_t* m_sockets; /* list of sockets */

	int m_commSocket; /* used for client */

	uint m_serverPort;
	char m_serverIP[20];

};

/* ~~~ Internal function forward declaration ~~~~~~~~~~~~~~~~~~~~~~~~ */

static bool IsStructValid(TCP_t* _TCP);
static bool IsConnected(TCP_t* _TCP);

static bool ServerSetup(TCP_t* _TCP);
static bool SetSocketBlockingEnabled(int fd, bool blocking);
static bool IsFail_nonBlocking(int _result);
static void sanity_check(char* _string, uint _size, char _replaceWith);

/* ~~~ API function ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

TCP_t* TCP_CreateServer(uint _port)
{
	TCP_t* aTCP = 0;
	aTCP = malloc(1 * sizeof(TCP_t) );
	if (!aTCP)
	{
		/* ERROR */
		return NULL;
	}

	aTCP->m_serverPort = _port;
	aTCP->m_connectedNum = 0;
	aTCP->m_commSocket = 0;
	aTCP->m_connectionCapacity = MAX_CLIENTS_NUM;

	/* setSocket. */
	aTCP->m_listenSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (SetSocketBlockingEnabled(aTCP->m_listenSocket, FALSE) < 0)
	{
		perror("Socket Failed");
		free(aTCP);
		return NULL;
	}

	if (! ServerSetup(aTCP) )
	{
		perror("ServerSetup Failed");
		free(aTCP);
		return NULL;
	}

	aTCP->m_sockets = list_new();
	if (! aTCP->m_sockets)
	{
		perror("List_Create Failed");
		free(aTCP);
		return NULL;
	}

	aTCP->m_magicNumber = ALIVE_MAGIC_NUMBER;

	return aTCP;
}

TCP_t* TCP_CreateClient(char* _ServerIP, uint _serverPort)
{
	TCP_t* aTCP = 0;
	aTCP = malloc(1 * sizeof(TCP_t) );
	if (!aTCP)
	{
		/* ERROR */
		return NULL;
	}

	strcpy(aTCP->m_serverIP , _ServerIP);
	aTCP->m_serverPort = _serverPort;
	aTCP->m_connectedNum = 0;
	aTCP->m_sockets = NULL;

	aTCP->m_commSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (aTCP->m_commSocket < 0)
	{
		perror("Socket Failed");
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

void TCP_DestroyServer(TCP_t* _TCP)
{
	if ( !IsStructValid(_TCP) )
	{
		return;
	}

	_TCP->m_magicNumber = DEAD_MAGIC_NUMBER;

	/* close the socket */
	close (_TCP->m_commSocket); /* not needed*/
	close (_TCP->m_listenSocket);

	if ( _TCP->m_sockets )
	{
		list_node_t *node;
		list_iterator_t *it = list_iterator_new(_TCP->m_sockets, LIST_HEAD);
		while ((node = list_iterator_next(it)))
		{
			close (*(int*)(node->val));
		}
		list_destroy(_TCP->m_sockets);
	}

	free(_TCP);
	return;
}

void TCP_DestroyClient(TCP_t* _TCP)
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


bool ServerSetup(TCP_t* _TCP)
{
	/* Reusing port */
	int optval = 1;
	if ( setsockopt(_TCP->m_listenSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval) ) < 0)
	{
		perror("Socket setsockopt Failed");
		return FALSE;
	}

	/* bind */
	struct sockaddr_in sIn;
	memset(&sIn , 0 , sizeof(sIn) );
	sIn.sin_family = AF_INET;
	sIn.sin_addr.s_addr = INADDR_ANY;
	sIn.sin_port = htons(_TCP->m_serverPort);

	/*Bind socket with address struct*/
	if (0 < bind(_TCP->m_listenSocket, (struct sockaddr *) &sIn, sizeof(sIn)) )
	{
		perror("Bind ServerConnect Failed.");
		return FALSE;
	}

	/* set socket to listen to new client */
	if ( listen(_TCP->m_listenSocket , BACK_LOG_CAPACITY) < 0 )
	{
		perror("Bind ServerConnect Failed.");
		return FALSE;
	}

	return TRUE;
}

bool TCP_ServerConnect(TCP_t* _TCP)
{
	if (! IsStructValid(_TCP) )
	{
		return FALSE;
	}

	/* too many open connections */

	struct sockaddr_in sIn;
	memset(&sIn , 0 , sizeof(sIn) );
	uint addr_len;
	addr_len = sizeof(sIn);
	int socket;
	socket = accept(_TCP->m_listenSocket,  (struct sockaddr *) &sIn, &addr_len ) ;

	/* got real new socket but over capacity, so close connection */
	if (0 < socket && _TCP->m_connectionCapacity <= _TCP->m_connectedNum)
	{
		#if !defined(NDEBUG) /* DEBUG */
    	printf("server has too many (%d) connection. dropping new socket #%d.\n", _TCP->m_connectedNum, socket);
		#endif

		close(socket);
		return FALSE;
	}

	if (socket > 0)
	{ /* Success accept link */

		SetSocketBlockingEnabled(socket, FALSE);

		/* add new socket to list of sockets */
		int* tmpSocket = malloc(sizeof(socket));
		*tmpSocket = socket;
		list_rpush(_TCP->m_sockets, list_node_new(tmpSocket));

		_TCP->m_connectedNum++;
		return TRUE;
	}
	else if ( IsFail_nonBlocking( socket) )
	{ /* Failed accept link */
		perror("TCP_ServerConnect accept Failed.");
		return FALSE;
	}
	else
	{ /* no one on the other side */
		return FALSE;
	}
}

bool TCP_ClientConnect(TCP_t* _TCP)
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

bool TCP_ServerDisconnect(TCP_t* _TCP, uint _socketNum)
{
	if (! IsStructValid(_TCP) )
	{
		return FALSE;
	}
	if (NULL == _TCP->m_sockets)
	{
		/* no list means its client and not server */
		return FALSE;
	}

	list_node_t *node;
	list_iterator_t *it = list_iterator_new(_TCP->m_sockets, LIST_HEAD);
	while ((node = list_iterator_next(it)))
	{
		if ( *(int*)(node->val) == _socketNum)
		{
			/* found the socket looking to remove			 */
			close(*(int*)(node->val));

			list_remove(_TCP->m_sockets, node);
			_TCP->m_connectedNum--;
			return TRUE;
		}
	}
	return FALSE;
}

int TCP_DoServer(TCP_t* _TCP, actionFunc _appFunc)
{
	char buffer[BUFFER_MAX_SIZE];
	int resultSize = 0;

	if (! IsStructValid(_TCP) || NULL == _appFunc)
	{
		return FALSE;
	}

	while( TRUE )
	{
		do {
			TCP_ServerConnect(_TCP);
		} while(! _TCP->m_connectedNum ); /* don't continue id there is not even one connection */

		list_node_t *node;
		list_iterator_t *it = list_iterator_new(_TCP->m_sockets, LIST_HEAD);
		while ((node = list_iterator_next(it)))
		{
			resultSize = TCP_Recive(_TCP, *(int*)(node->val), buffer, BUFFER_MAX_SIZE);
			if (resultSize == 0)
			{
				/* socket was closed */
				break;
			}
			else if (resultSize > 0)
			{
				_appFunc(buffer, resultSize, NULL);

				resultSize = TCP_Send(_TCP, *(int*)(node->val), buffer, resultSize);
			}
		}
	}

	return TRUE;
}


int TCP_Send(TCP_t* _TCP, uint _socketNum, void* _msg, uint _msgLength)
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
	sent_bytes = send( _socketNum, _msg, _msgLength, 0 );

	if (0 > sent_bytes)
	{
		perror("Send Failed");
	}

	return sent_bytes;
}



int TCP_Recive(TCP_t* _TCP, uint _socketNum, void* _buffer, uint _bufferMaxLength)
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

    nBytesRead = recv( _socketNum, _buffer, _bufferMaxLength , 0 );

    if (nBytesRead == 0)
    {
		#if !defined(NDEBUG) /* DEBUG */
    	printf("socket #%d was closed on client side. disconnecting from server.\n", _socketNum);
		#endif

    	TCP_ServerDisconnect(_TCP, _socketNum);
    }
    else if ( IsFail_nonBlocking(nBytesRead) )
    {
    	perror("Read Failed.");
    }

    sanity_check(_buffer, _bufferMaxLength, '_');

    return nBytesRead;
}


int TCP_ClientGetSocket(TCP_t* _TCP)
{
	if ( !IsStructValid(_TCP) || ! IsConnected(_TCP))
	{
		return GENERAL_ERROR;
	}


	return _TCP->m_commSocket;
}




/* ~~~ Internal function  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool IsStructValid(TCP_t* _TCP)
{
	return !(NULL == _TCP || ALIVE_MAGIC_NUMBER != _TCP->m_magicNumber);
}

bool IsConnected(TCP_t* _TCP)
{
	return (bool) _TCP->m_connectedNum;
}



/** Returns true on success, or false if there was an error */
static bool SetSocketBlockingEnabled(int fd, bool blocking)
{
	if (fd < 0) return FALSE;

#ifdef WIN32
	unsigned long mode = blocking ? 0 : 1;
	return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? TRUE : FALSE;
#else
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) return FALSE;
	flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
	return (fcntl(fd, F_SETFL, flags) == 0) ? TRUE : FALSE;
#endif
}

static bool IsFail_nonBlocking(int _result)
{
	return (0 > _result && errno != EAGAIN && errno != EWOULDBLOCK);
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
