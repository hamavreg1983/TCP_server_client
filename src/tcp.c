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
#include <sys/types.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <arpa/inet.h>    /* close */
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "list.h"
#include "tcp.h"

/* ~~~ Defines ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#define ALIVE_MAGIC_NUMBER	0xfadeface
#define DEAD_MAGIC_NUMBER	0xdeadface

#define GENERAL_ERROR -9
#define BACK_LOG_CAPACITY 128

#define IS_NON_BLOCKING_METHOD FALSE

/* ~~~ Global ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* ~~~ Struct ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

struct TCP_S
{
	int m_magicNumber;

	int m_connectionCapacity;
	int m_connectedNum; /* count the amount of open connections */
	int m_listenSocket;
	list_t* m_sockets; /* list of sockets */

	uint m_serverPort;
	char m_serverIP[INET6_ADDRSTRLEN];

	bool m_isServerRun;

	userActionFunc m_reciveDataFunc;
	clientConnectionChangeFunc m_newConnectionFunc;
	clientConnectionChangeFunc m_closedConnectionFunc;
	errorFunc m_errorFunc;

};

/* ~~~ Internal function forward declaration ~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief when a new client is detected, this function is called and handles the connection on server side.
 * @param _TCP pointer to the struct
 * @return the status of return. TRUE when stopped normally or FALSE when failed
 */
bool TCP_Server_ConnectNewClient(TCP_S_t* _TCP);

/**
 * @brief when a client has disconnected is detected, this function is called and handles the connection on server side.
 * @param _TCP pointer to the struct
 * @param _socketNum the connection number (file descriptor socket) which need to disconnect
 * @return the status of return. TRUE when normally or FALSE when failed
 */
bool TCP_ServerDisconnectClient(TCP_S_t* _TCP, uint _socketNum);

bool SelectServer(TCP_S_t* _TCP);
bool NonBlockingServer(TCP_S_t* _TCP);
int SetupSelect(int _listenSocket, list_t* _socketList, fd_set* _readfds);
int ReadFromSelect(TCP_S_t* _TCP, fd_set* _readfds);

static bool IsStructValid(TCP_S_t* _TCP);
static bool IsConnected(TCP_S_t* _TCP);

static bool ServerSetup(TCP_S_t* _TCP);
static bool SetSocketBlockingEnabled(int fd, bool blocking);
static bool IsFail_nonBlocking(int _result);

static void sanity_check(char* _string, uint _size, char _replaceWith);

/* ~~~ API function ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

TCP_S_t* TCP_CreateServer(uint _port, const char* _serverIP, uint _maxConnections,
		userActionFunc _reciveDataFunc,
		clientConnectionChangeFunc _newClientConnected,
		clientConnectionChangeFunc _clientDissconected,
		errorFunc _errorFunc
		)
{
	TCP_S_t* aTCP = 0;

	if (_reciveDataFunc == NULL)
	{
		return NULL;
	}

	aTCP = malloc(1 * sizeof(TCP_S_t) );
	if (!aTCP)
	{
		/* ERROR */
		return NULL;
	}

	if (_serverIP != NULL)
	{
		strncpy(aTCP->m_serverIP , _serverIP , INET6_ADDRSTRLEN);
	}
	else
	{
		aTCP->m_serverIP[0] = '\0';
	}

	aTCP->m_serverPort = _port;
	aTCP->m_connectedNum = 0;
	aTCP->m_connectionCapacity = _maxConnections;

	aTCP->m_reciveDataFunc = _reciveDataFunc;
	aTCP->m_newConnectionFunc = _newClientConnected;
	aTCP->m_closedConnectionFunc = _clientDissconected;
	aTCP->m_errorFunc = _errorFunc;

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
		close(aTCP->m_listenSocket);
		free(aTCP);
		return NULL;
	}

	aTCP->m_magicNumber = ALIVE_MAGIC_NUMBER;

	return aTCP;
}

void TCP_DestroyServer(TCP_S_t* _TCP)
{
	if ( !IsStructValid(_TCP) )
	{
		return;
	}

	_TCP->m_magicNumber = DEAD_MAGIC_NUMBER;

	/* close the socket */
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

bool ServerSetup(TCP_S_t* _TCP)
{
	/* setSocket. */
	_TCP->m_listenSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (IS_NON_BLOCKING_METHOD == TRUE)
	{
		if (SetSocketBlockingEnabled(_TCP->m_listenSocket, FALSE) < 0)
		{
			perror("Socket Failed");
			return FALSE;
		}
	}

	/* Reusing port */
	int optval = 1;
	if ( setsockopt(_TCP->m_listenSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval) ) < 0)
	{
		perror("Socket setsockopt Failed");
		close(_TCP->m_listenSocket);
		return FALSE;
	}

	/* bind */
	struct sockaddr_in sIn;
	memset(&sIn , 0 , sizeof(sIn) );
	sIn.sin_family = AF_INET;
	if ( strlen(_TCP->m_serverIP) == 0)
	{
		sIn.sin_addr.s_addr = INADDR_ANY;
	} else {
		sIn.sin_addr.s_addr = inet_addr(_TCP->m_serverIP);
	}
	sIn.sin_port = htons(_TCP->m_serverPort);

	/*Bind socket with address struct*/
	if (0 < bind(_TCP->m_listenSocket, (struct sockaddr *) &sIn, sizeof(sIn)) )
	{
		perror("Bind ServerConnect Failed.");
		close(_TCP->m_listenSocket);
		return FALSE;
	}

	/* set socket to listen to new client */
	if ( listen(_TCP->m_listenSocket , BACK_LOG_CAPACITY) < 0 )
	{
		perror("Listen ServerConnect Failed.");
		close(_TCP->m_listenSocket);
		return FALSE;
	}

	return TRUE;
}

bool TCP_Server_ConnectNewClient(TCP_S_t* _TCP)
{
	if (! IsStructValid(_TCP) )
	{
		return FALSE;
	}

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

    	if(_TCP->m_errorFunc)
    	{
    		_TCP->m_errorFunc(TOO_MANY_CONNECTION, socket, NULL);
    	}

		close(socket);
		return FALSE;
	}

	if (socket > 0)
	{ /* Success accept link */

		if (IS_NON_BLOCKING_METHOD == TRUE)
		{
			SetSocketBlockingEnabled(socket, FALSE);
		}

		/* add new socket to list of sockets */
		int* tmpSocket = malloc(sizeof(socket));
		if (tmpSocket == NULL)
		{
			close(socket);
			return FALSE;
		}
		*tmpSocket = socket;
		if (! list_rpush(_TCP->m_sockets, list_node_new(tmpSocket)) )
		{
			/* check list fail */
			close(socket);
			free(tmpSocket);
			return FALSE;
		}

		_TCP->m_connectedNum++;

		if (_TCP->m_newConnectionFunc)
		{
			/* if user provide a function to invoke when new client connected */
			_TCP->m_newConnectionFunc(socket, NULL);
		}

		return TRUE;
	}
	else if ( IsFail_nonBlocking( socket) )
	{ /* Failed accept link */
		perror("TCP_ServerConnect accept Failed.");
		return FALSE;
	}
	else
	{ /* no one on the other side, all is well */
		return FALSE;
	}
}

bool TCP_ServerDisconnectClient(TCP_S_t* _TCP, uint _socketNum)
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

			if (_TCP->m_closedConnectionFunc)
			{
				/* if user provide a function to invoke when a client disconnect */
				_TCP->m_closedConnectionFunc( (*(int*)(node->val)) , NULL);
			}

			close(*(int*)(node->val));
			list_remove(_TCP->m_sockets, node);
			_TCP->m_connectedNum--;

			return TRUE;
		}
	}
	return FALSE;
}

bool TCP_RunServer(TCP_S_t* _TCP)
{
	if (! IsStructValid(_TCP))
	{
		return FALSE;
	}

	if (IS_NON_BLOCKING_METHOD == TRUE)
	{
		return NonBlockingServer(_TCP);
	}
	else
	{
		return SelectServer(_TCP);
	}
}

bool NonBlockingServer(TCP_S_t* _TCP)
{
	char buffer[BUFFER_MAX_SIZE];
	int resultSize = 0;

	_TCP->m_isServerRun = TRUE;

	while( _TCP->m_isServerRun )
	{
		while ( TCP_Server_ConnectNewClient(_TCP) == TRUE)
		{
			/* keep on accepting all waiting client while there are some */
		}

		list_node_t *node;
		list_iterator_t *it = list_iterator_new(_TCP->m_sockets, LIST_HEAD);
		while ((node = list_iterator_next(it)))
		{
			resultSize = TCP_Recive(*(int*)(node->val), buffer, BUFFER_MAX_SIZE);
			if (resultSize == 0)
			{
				/* socket was closed */
				break;
			}
			else if (resultSize > 0)
			{
				_TCP->m_reciveDataFunc(buffer, resultSize, *(int*)(node->val), NULL);
			}
		}
	}

	return TRUE;
}

bool TCP_StopServer(TCP_S_t* _TCP)
{
	if (! IsStructValid(_TCP) || _TCP->m_isServerRun == FALSE)
	{
		return FALSE;
	}
	_TCP->m_isServerRun = FALSE;
	return TRUE;
}

int TCP_Send(uint _socketNum, void* _msg, uint _msgLength)
{
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



int TCP_Recive(uint _socketNum, void* _buffer, uint _bufferMaxLength)
{
	int nBytesRead;

/*	if ( !IsStructValid(_TCP) || ! IsConnected(_TCP))
	{
		return GENERAL_ERROR;
	}*/
	if ( NULL == _buffer)
	{
		return GENERAL_ERROR;
	}

    nBytesRead = recv( _socketNum, _buffer, _bufferMaxLength , 0 );

    if (nBytesRead == 0)
    {
		/* #if !defined(NDEBUG)
    	printf("socket #%d was closed on client side. disconnecting from server.\n", _socketNum);
		#endif */

    	/* TCP_ServerDisconnectClient(_TCP, _socketNum); */
    }
    else if ( IsFail_nonBlocking(nBytesRead) )
    {
    	perror("Read Failed.");
    }
    else
    { /* good read value */
    	sanity_check(_buffer, _bufferMaxLength, '_');
    }

    return nBytesRead;
}

bool SelectServer(TCP_S_t* _TCP)
{
	int activity , sd;
	int max_sd;

	//set of socket descriptors
	fd_set readfds;

	//if valid socket descriptor then add to read list
	FD_SET( sd , &readfds);

	_TCP->m_isServerRun = TRUE;
	while( _TCP->m_isServerRun )
	{
		max_sd = SetupSelect( _TCP->m_listenSocket, _TCP->m_sockets, &readfds);

		//wait for an activity on one of the sockets , timeout is NULL ,
		//so wait indefinitely
		activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

		if ((activity < 0) && (errno!=EINTR)) /* change to my fucntion that check if real failed */
		{
			perror("select error");
			return FALSE;
		}

		//If something happened on the master socket ,
		//then its an incoming connection
		if (FD_ISSET(_TCP->m_listenSocket, &readfds))
		{
			/* call server connect function */
			if (IS_NON_BLOCKING_METHOD == FALSE)
			{
				TCP_Server_ConnectNewClient(_TCP);
			}
			else
			{
				while ( TCP_Server_ConnectNewClient(_TCP) == TRUE)
				{
					/* keep on accepting all waiting client while there are some */
				}
			}
		}

		/* find the sockets that woke the selector, read from it and activate user function */
		ReadFromSelect(_TCP, &readfds);
	}

	return TRUE;
}

int SetupSelect(int _listenSocket, list_t* _socketList, fd_set* _readfds)
{
	int max_sd, sd;
	list_iterator_t* itr;
	list_node_t *node;

	//clear the socket set
	FD_ZERO(_readfds);

	//add master socket to set
	FD_SET(_listenSocket, _readfds);
	max_sd = _listenSocket;

	//add child sockets to set
	itr = list_iterator_new(_socketList, LIST_HEAD);
	while ((node = list_iterator_next(itr)))
	{
		//socket descriptor
		sd = *(int*)(node->val);

		//if valid socket descriptor then add to read list
		FD_SET( sd , _readfds);

		//highest file descriptor number, need it for the select function
		if(sd > max_sd)
		{
			max_sd = sd;
		}
	}
	return max_sd;
}

int ReadFromSelect(TCP_S_t* _TCP, fd_set* _readfds)
{
	int sd;
	int resultSize;
	list_iterator_t* itr;
	list_node_t *node;
	char buffer[BUFFER_MAX_SIZE];

	//else its some IO operation on some other socket
	itr = list_iterator_new(_TCP->m_sockets, LIST_HEAD);
	while ((node = list_iterator_next(itr)))
	{
		sd = *(int*)(node->val);

		if (FD_ISSET( sd , _readfds))
		{
			/* found the socket that woke */
			resultSize = TCP_Recive(*(int*)(node->val), buffer, BUFFER_MAX_SIZE);
			if (resultSize == 0)
			{
				/* socket was closed */
				if (! TCP_ServerDisconnectClient(_TCP, sd) )
				{
					perror("problam removing empty client");
				}
			}
			else if (resultSize > 0)
			{
				_TCP->m_reciveDataFunc(buffer, resultSize, *(int*)(node->val), NULL);
			}
			else
			{
				/* Do nothing. it was dealt with.
				 *perror("Error");
				 */
			}
		}
	}
	return TRUE;
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
