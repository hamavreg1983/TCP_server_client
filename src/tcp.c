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
#include <sys/time.h>  /* FD_SET, FD_ISSET, FD_ZERO macros , timeval */
#include <sys/select.h>
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
#define SI_MAGIC_NUMBER	0xdeaddead

#define GENERAL_ERROR -9
#define BACK_LOG_CAPACITY 128

/* This server can work on two methods. if TRUE a busy-wait read would occur. if FALSE a select waiting on all socket would occur. */
#define IS_NON_BLOCKING_METHOD FALSE

typedef struct timeval timeval_t;


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

	uint m_timeoutMS;

	bool m_isServerRun;

	userActionFunc m_reciveDataFunc;
	clientConnectionChangeFunc m_newConnectionFunc;
	clientConnectionChangeFunc m_closedConnectionFunc;
	errorFunc m_errorFunc;

	void* m_contex;

};

typedef struct SocketInfo
{
	int m_magicNumber;

	int m_socketFD;
	timeval_t m_timeToDie;
} SocketInfo_t ;

/* ~~~ Internal function forward declaration ~~~~~~~~~~~~~~~~~~~~~~~~ */

/**
 * @brief when a new client is detected, this function is called and handles the connection on server side.
 * @param _TCP pointer to the struct
 * @return the status of return. TRUE when stopped normally or FALSE when failed
 */
static bool TCP_Server_ConnectNewClient(TCP_S_t* _TCP);

/**
 * @brief when a client has disconnected is detected, this function is called and handles the connection on server side.
 * @param _TCP pointer to the struct
 * @param _socketNum the connection number (file descriptor socket) which need to disconnect
 * @return the status of return. TRUE when normally or FALSE when failed
 */
static bool TCP_ServerDisconnectClient(TCP_S_t* _TCP, uint _socketNum);

static bool SelectServer(TCP_S_t* _TCP);
static bool NonBlockingServer(TCP_S_t* _TCP);
static int SetupSelect(int _listenSocket, list_t* _socketList, fd_set* _readfds);
static int ReadFromSelect(TCP_S_t* _TCP, fd_set* _readfds);

static bool IsStructValid(TCP_S_t* _TCP);
static bool IsConnected(TCP_S_t* _TCP);

static bool ServerSetup(TCP_S_t* _TCP);
static bool SetSocketBlockingEnabled(int fd, bool blocking);
static bool IsFail_nonBlocking(int _result);

static bool MoveNodeToHead(list_t* _socketsContiner, list_node_t* node, uint _timeoutMS);
static bool KillOldestClient(TCP_S_t* _TCP);
static timeval_t DealWithTimeout(TCP_S_t* _TCP);

static SocketInfo_t* CreateSocketInfo(int _socket, uint _timeoutMS);
static void DestorySocketInfo(SocketInfo_t* _SI);
static int getSocket(list_node_t* _node);
/* static int setSocket(list_node_t* _node, int _socketNum); */ /* un used */
static timeval_t getTimeout(list_node_t* _node);
static timeval_t setTimeout(list_node_t* _node, timeval_t _when2die);
static timeval_t WhenIsTime2Die(uint _timeoutMS);

static void sanity_check(char* _string, uint _size, char _replaceWith);

/* ~~~ API function ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

TCP_S_t* TCP_CreateServer(uint _port, const char* _serverIP, uint _maxConnections, uint _timeoutMS,
		userActionFunc _reciveDataFunc,
		clientConnectionChangeFunc _newClientConnected,
		clientConnectionChangeFunc _clientDissconected,
		errorFunc _errorFunc,
		void* _contex
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
	aTCP->m_timeoutMS = _timeoutMS;

	aTCP->m_reciveDataFunc = _reciveDataFunc;
	aTCP->m_newConnectionFunc = _newClientConnected;
	aTCP->m_closedConnectionFunc = _clientDissconected;
	aTCP->m_errorFunc = _errorFunc;
	aTCP->m_contex = _contex;

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
		list_node_t* node;
		list_iterator_t *it = list_iterator_new(_TCP->m_sockets, LIST_HEAD);
		while ((node = list_iterator_next(it)))
		{
			/* close (*(int*)(node->val)); */
			close ( getSocket(node) );
		}
		list_destroy(_TCP->m_sockets);
	}

	free(_TCP);
	return;
}

bool TCP_RunServer(TCP_S_t* _TCP)
{
	if (! IsStructValid(_TCP) && ! IsConnected(_TCP))
	{
		return FALSE;
	}

	printf("Server is Ready. Waiting for clients...\n");

	if (IS_NON_BLOCKING_METHOD == TRUE)
	{
		return NonBlockingServer(_TCP);
	}
	else
	{
		return SelectServer(_TCP);
	}
}

static bool NonBlockingServer(TCP_S_t* _TCP)
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
		list_iterator_t *itr = list_iterator_new(_TCP->m_sockets, LIST_HEAD);
		while ((node = list_iterator_next(itr)))
		{
			resultSize = TCP_Recive( getSocket(node) , buffer, BUFFER_MAX_SIZE);
			if (resultSize == 0)
			{
				/* socket was closed */
				break;
			}
			else if (resultSize > 0)
			{
				_TCP->m_reciveDataFunc(buffer, resultSize, getSocket(node) , _TCP->m_contex);
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

	if ( NULL == _buffer)
	{
		return GENERAL_ERROR;
	}

    nBytesRead = recv( _socketNum, _buffer, _bufferMaxLength , 0 );

    if (nBytesRead == 0)
    {
		/* socket was closed on client side. do nothing, transfer 0 up a function where it would be disconnected */
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




/* ~~~ Internal function  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

bool IsStructValid(TCP_S_t* _TCP)
{
	return !(NULL == _TCP || ALIVE_MAGIC_NUMBER != _TCP->m_magicNumber);
}

bool IsConnected(TCP_S_t* _TCP)
{
	return (bool) _TCP->m_connectedNum;
}


static bool ServerSetup(TCP_S_t* _TCP)
{
	/* setSocket. */
	_TCP->m_listenSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (SetSocketBlockingEnabled(_TCP->m_listenSocket, FALSE) < 0)
	{
		perror("Socket Failed");
		return FALSE;
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
	if ( strlen(_TCP->m_serverIP) == 0 || _TCP->m_serverIP[0] == '\0')
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

static bool TCP_Server_ConnectNewClient(TCP_S_t* _TCP)
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
	SocketInfo_t* aSI;

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
		aSI = CreateSocketInfo(socket, _TCP->m_timeoutMS);
		if (!aSI)
		{
			close(socket);
			return FALSE;
		}

		if (! list_rpush(_TCP->m_sockets, list_node_new(aSI)) )
		{
			/* check list fail */
			close(socket);
			DestorySocketInfo(aSI);
			return FALSE;
		}

		_TCP->m_connectedNum++;

		if (_TCP->m_newConnectionFunc)
		{
			/* if user provide a function to invoke when new client connected */
			_TCP->m_newConnectionFunc(socket, _TCP->m_contex);
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

static bool TCP_ServerDisconnectClient(TCP_S_t* _TCP, uint _socketNum)
{
	if (! IsStructValid(_TCP) )
	{
		return FALSE;
	}
	if (NULL == _TCP->m_sockets)
	{
		/* no list means big probalmes */
		return FALSE;
	}

	/* TODO for better preformance search from tail to head as most of the sockes to remove are at the tail */
	list_node_t *node;
	list_iterator_t *it = list_iterator_new(_TCP->m_sockets, LIST_HEAD);
	while ((node = list_iterator_next(it)))
	{
		if ( getSocket(node) == _socketNum)
		{
			/* found the socket looking to remove			 */

			if (_TCP->m_closedConnectionFunc)
			{
				/* if user provide a function to invoke when a client disconnect */
				_TCP->m_closedConnectionFunc( getSocket(node) , _TCP->m_contex);
			}

			close( getSocket(node) );
			DestorySocketInfo(node->val); /* TODO might cause a core dump in next line */
			list_remove(_TCP->m_sockets, node);
			_TCP->m_connectedNum--;

			return TRUE;
		}
	}
	return FALSE;
}

static bool MoveNodeToHead(list_t* _socketsContiner, list_node_t* node, uint _timeoutMS)
{
	list_node_t* newNode;
	SocketInfo_t* SI;

	/* store value */
	int socketTemp = getSocket(node);
	SI = node->val;
	node->val = NULL;

	/* remove old node */
	list_remove(_socketsContiner, node);

	//setTimeout(node, WhenIsTime2Die(_timeoutMS) );

	/* TODO does the above line fits with the one below?! */

	/* add socket to list of sockets at head */
//	aSI = CreateSocketInfo(socketTemp, _timeoutMS);
//	if (!aSI)
//	{
//		return FALSE;
//	}

	/* create new node with old data */
	newNode = list_node_new(SI);
	/* Calculate and update timeout */
	setTimeout(newNode, WhenIsTime2Die(_timeoutMS) );

	if (! list_lpush(_socketsContiner, newNode ) )
	{
		/* check list fail */
		close( socketTemp);
		DestorySocketInfo(SI);
		return FALSE;
	}

	return TRUE;
}

static timeval_t setTimeout(list_node_t* _node, timeval_t _when2die)
{
	SocketInfo_t* tempSocketInfo = _node->val;

	tempSocketInfo->m_timeToDie = _when2die;

	return tempSocketInfo->m_timeToDie;
}

static bool SelectServer(TCP_S_t* _TCP)
{
	int activity;
	int max_sd;

	//set of socket descriptors
	fd_set readfds;

	timeval_t when2wakeup;

	_TCP->m_isServerRun = TRUE;
	while( _TCP->m_isServerRun )
	{
		/* TODO next line bracks code */
		when2wakeup = DealWithTimeout(_TCP); /* close sockets that are open for longer than timeout */
		KillOldestClient(_TCP); /* if capacity is full, close oldest connections */

		max_sd = SetupSelect( _TCP->m_listenSocket, _TCP->m_sockets, &readfds);

		//wait for an activity on one of the sockets , timeout is NULL ,
		//so wait indefinitely
		activity = select( max_sd + 1 , &readfds , NULL , NULL , &when2wakeup);
		//activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL); /* after the timeout replace this line with the one above */

		if ((activity < 0) && (errno!=EINTR)) /* change to my function that check if real failed */
		{
			perror("select error");
			return FALSE;
		}
		else if (activity == 0)
		{
			/* select woke-up because of timeout. */

		}
		else{
			/* activity > 0 means found real activity. */

			//If something happened on the master socket ,
			//then its an incoming connection
			if (FD_ISSET(_TCP->m_listenSocket, &readfds))
			{
				/* call server connect function */
				while ( TCP_Server_ConnectNewClient(_TCP) == TRUE)
				{
					/* keep on accepting all waiting client while there are some */
				}
			}

			/* find the sockets that woke the selector, read from it and activate user function */
			ReadFromSelect(_TCP, &readfds);
		}
	}

	return TRUE;
}

static timeval_t DealWithTimeout(TCP_S_t* _TCP)
{
	timeval_t currentTime;
	timeval_t time2die;
	timeval_t when2wakeup = {60,0}; /* if list is empty this defualt value would be used as max sleep time */
	list_node_t* tailNode;

	if ( gettimeofday(&currentTime, NULL) )
	{
		perror("failed to get time.\n");
	}

	while (	(tailNode = list_at(_TCP->m_sockets, -1) ) != list_at(_TCP->m_sockets, 0))
	{
		/* test tailNode is not endNode = list is empty */

		time2die = getTimeout(tailNode);
		timersub(&time2die, &currentTime, &when2wakeup);

		if ( timercmp(&time2die, &currentTime, >) )
		{
			/* time2die is later than current time */
			/* just return when to wakeup */

			return when2wakeup;
		}
		else
		{
			/* last node socket should be removed */
			if (! TCP_ServerDisconnectClient(_TCP, getSocket(tailNode) ) )
			{
				perror("Can't remove at ShouldKillClient");
			}

			/* recursive loop until no more nodes to kill because of time */

		}
	}
	return when2wakeup;
}

static int SetupSelect(int _listenSocket, list_t* _socketList, fd_set* _readfds)
{
	int max_sd, sd;
	list_iterator_t* itr;
	list_node_t *node;

	//clear the socket set
	FD_ZERO(_readfds);

	//add master socket to set
	FD_SET(_listenSocket, _readfds);
	max_sd = _listenSocket;
	FD_SET(0, _readfds);

	//add child sockets to set
	itr = list_iterator_new(_socketList, LIST_HEAD);
	while ((node = list_iterator_next(itr)))
	{
		//socket descriptor
		sd = getSocket(node);

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

static int ReadFromSelect(TCP_S_t* _TCP, fd_set* _readfds)
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
		sd = getSocket(node);

		if (FD_ISSET( sd , _readfds))
		{
			/* found the socket that woke */
			resultSize = TCP_Recive( getSocket(node), buffer, BUFFER_MAX_SIZE);
			if (resultSize == 0)
			{
				/* socket was closed */
				if (! TCP_ServerDisconnectClient(_TCP, sd) )
				{
					perror("Problem removing empty client");
				}
			}
			else if (resultSize > 0)
			{
				_TCP->m_reciveDataFunc(buffer, resultSize, getSocket(node), _TCP->m_contex);

				if (! MoveNodeToHead(_TCP->m_sockets, node, _TCP->m_timeoutMS) )
				{
					perror("Error UpdateSocketTimeout.\n");
				}
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

static bool KillOldestClient(TCP_S_t* _TCP)
{
	/* TODO remove hardcoded value */

	if (_TCP->m_connectionCapacity * 0.99 <= _TCP->m_connectedNum)
	{
		/* server is almost full. lets disconnects the oldest connections */
		list_node_t* tailNode = list_at(_TCP->m_sockets, -1);

		if (! TCP_ServerDisconnectClient(_TCP, getSocket(tailNode) ) )
		{
			perror("Can't remove at ShouldKillClient");
		}
	}

	return TRUE;
}

static SocketInfo_t* CreateSocketInfo(int _socket, uint _timeoutMS)
{
	SocketInfo_t* aSI = malloc(1 * sizeof(SocketInfo_t) );
	if (! aSI)
	{
		return NULL;
	}

	aSI->m_socketFD = _socket;
	aSI->m_timeToDie = WhenIsTime2Die(_timeoutMS);

	aSI->m_magicNumber = SI_MAGIC_NUMBER;

	return aSI;
}

static void DestorySocketInfo(SocketInfo_t* _SI)
{
	if (NULL == _SI || _SI->m_magicNumber != SI_MAGIC_NUMBER)
	{
		return;
	}

	_SI->m_magicNumber = -1;
	close(_SI->m_socketFD);
	free(_SI);
	return;
}

static int getSocket(list_node_t* _node)
{
	if (NULL == _node)
	{
		return -1;
	}

	SocketInfo_t* tempSocketInfo = _node->val;
	return tempSocketInfo->m_socketFD;
}

//static int setSocket(list_node_t* _node, int _socketNum)
//{
//	if (NULL == _node)
//	{
//		return -1;
//	}
//
//	SocketInfo_t* tempSocketInfo = _node->val;
//	tempSocketInfo->m_socketFD = _socketNum;
//	return tempSocketInfo->m_socketFD;
//}

static timeval_t getTimeout(list_node_t* _node)
{
		if (NULL == _node)
		{
			timeval_t temp = {0,0};
			return temp;
		}

		SocketInfo_t* tempSocketInfo = _node->val;
		return tempSocketInfo->m_timeToDie;
}

static timeval_t WhenIsTime2Die(uint _timeoutMS)
{
	timeval_t currentTime;
	timeval_t timeout;
	timeval_t time2die;

	if ( gettimeofday(&currentTime, NULL) )
	{
		perror ( "Can't get current time.\n" );
	}
	timeout.tv_sec = _timeoutMS/1000;

	timeradd(&currentTime, &timeout, &time2die);

	return time2die;
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
