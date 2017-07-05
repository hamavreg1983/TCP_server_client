/*
 * client.c
 *
 *  Created on: Jul 4, 2017
 *      Author: uv
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#include "tcp.h"

#define MAX_MSG_SIZE 1024
#define CLIENT_NUM 1500

/* global for sigaction */
TCP_t** g_object2Destroy = NULL;

void sanity_check(char* _string, uint _size, char _replaceWith);

void sigAbortHandler(int dummy)
{
	int i;
	printf("\nFound Sig Cleaning and exit\n\n");
	if (NULL != g_object2Destroy)
	{
		for (i=0 ; i < CLIENT_NUM ; ++i)
		{
			TCP_DestroyClient(g_object2Destroy[i]);
		}
	}

    _exit(2);
}

int main(int argc, char* argv[])
{
	uint serverPort = 4848;  		/* Default value */
	char serverIP[16] = "127.0.0.1"; /* Default value */
	char msg[MAX_MSG_SIZE];			/* Default value */
	void* buffer[MAX_MSG_SIZE];
	int clientNum = CLIENT_NUM;
	int socket;
	int i;
	TCP_t* clientContiner[CLIENT_NUM];
	g_object2Destroy = clientContiner;
	float probabilty;

	struct sigaction psa;
	psa.sa_handler = sigAbortHandler;
	sigaction(SIGINT, &psa, NULL);

	if (argc == 3)
	{
		strcpy(serverIP , argv[1]);
		serverPort = atoi(argv[2]) ;
	}

	time_t t;
   /* Intializes random number generator */
   srand((unsigned) time(&t));

	printf("--START--\n");

	for (i=0 ; i < clientNum ; ++i)
	{
		clientContiner[i] = NULL;

	}

	strcpy(msg, "hello World");
	int sent_bytes;
	int recv_bytes;
	while ( TRUE )
	{
		printf("Loop test\n");
		for (i = 0; i < clientNum; ++i)
		{
			probabilty = (rand() % 100) / 100.0;

			if (NULL == clientContiner[i]) /* unConnect */
			{
				if (probabilty < 0.3)
				{
					/* create and connect */
					clientContiner[i] = TCP_CreateClient(serverIP, serverPort);
					printf("client #%d connected.\n", i);
				}
			}
			else /* connected */
			{
				if (probabilty < 0.1)
				{
					/* disconnect and destroy */
					TCP_DestroyClient(clientContiner[i]);
					clientContiner[i] = NULL;
					printf("client #%d disconnected.\n", i);
				}
				else if(probabilty > 0.7)
				{
					/* send recive */
					socket = TCP_ClientGetSocket(clientContiner[i]);

					sent_bytes = TCP_Send(clientContiner[i], socket, msg, strlen(msg) + 1 );
					if (0 > sent_bytes)
					{
						printf("\nError. send %d bytes", sent_bytes);
					}
					else
					{
						recv_bytes = TCP_Recive(clientContiner[i], socket, buffer, MAX_MSG_SIZE);
						if (0 == recv_bytes)
						{
							printf("server closed connection. exiting.\n");
							TCP_DestroyClient(clientContiner[i]);
							clientContiner[i] = NULL;
							/* break; */
						}
						printf("recived(%d): %s.\n", recv_bytes, (char*) buffer);

					}
				}
			}
		}
	}


	for (i=0 ; i < clientNum ; ++i)
	{
		TCP_DestroyClient(clientContiner[i]);
	}
	g_object2Destroy = NULL;

	printf("--END--\n");
}


void sanity_check(char* _string, uint _size, char _replaceWith)
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
