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

#include "tcp_client.h"

#define MAX_MSG_SIZE 1024
#define CLIENT_NUM 1010

/* global for sigaction */
bool g_isClientRun = TRUE;

void sanity_check(char* _string, uint _size, char _replaceWith);
void RandomMSG(char* _msg, uint _maxLength);

void sigAbortHandler(int dummy)
{
	const char notify[] = "\nGot Signal, lets Clean and exit\n\n";
	write(STDOUT_FILENO, notify, strlen(notify));

	g_isClientRun = FALSE;

    return;
}

int main(int argc, char* argv[])
{
	uint serverPort = 4848;  		/* Default value */
	char serverIP[16] = "127.0.0.1"; /* Default value */
	char msg[MAX_MSG_SIZE];			/* Default value */
	void* buffer[MAX_MSG_SIZE];
	int clientNum = CLIENT_NUM;
	int i;
	TCP_S_t* clientContiner[CLIENT_NUM];
	float probabilty;

	struct sigaction psa;
	psa.sa_handler = sigAbortHandler;
	sigaction(SIGINT, &psa, NULL);

	if (argc == 3)
	{
		strcpy(serverIP , argv[1]);
		serverPort = atoi(argv[2]) ;
	}

   /* Intializes random number generator */
	srand((unsigned int) time(NULL));

	printf("--START--\n");

	for (i=0 ; i < clientNum ; ++i)
	{
		clientContiner[i] = NULL;

	}

	int sent_bytes;
	int recv_bytes;
	while ( g_isClientRun )
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

					RandomMSG(msg, MAX_MSG_SIZE);

					sent_bytes = TCP_ClientSend(clientContiner[i], msg, strlen(msg) + 1 );
					if (0 > sent_bytes)
					{
						printf("\nError. send %d bytes", sent_bytes);
					}
					else
					{
						recv_bytes = TCP_ClientRecive(clientContiner[i], buffer, MAX_MSG_SIZE);
						if (0 == recv_bytes)
						{
							printf("server closed connection. quitting client.\n");
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

void RandomMSG(char* _msg, uint _maxLength)
{
	char header[] = "Start MSG:";
	char spacer[] = "^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla^bla";
	char footer[] = "END";

	int probabilty = (rand()%30);

	strcpy(_msg, header);
	memcpy(_msg + sizeof(header)-1, spacer, probabilty);
	memcpy(_msg + sizeof(header)-1 + probabilty, footer, sizeof(footer));

	if (strlen(_msg) > _maxLength)
	{
		printf("i just overflowed my own content");
		exit(3);
	}
	return;
}
