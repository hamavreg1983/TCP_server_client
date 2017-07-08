/*
 * client.c
 *
 *  Created on: Jul 4, 2017
 *      Author: uv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcp_client.h"

#define MAX_MSG_SIZE 1024

static void sanity_check(char* _string, uint _size, char _replaceWith);


int main(int argc, char* argv[])
{
	uint serverPort = 4848;  		/* Default value */
	char serverIP[16] = "127.0.0.1"; /* Default value */
	char msg[MAX_MSG_SIZE];			/* Default value */
	void* buffer[MAX_MSG_SIZE];

	if (argc == 3)
	{
		strcpy(serverIP , argv[1]);
		serverPort = atoi(argv[2]) ;
	}

	printf("--START--\n");
	TCP_C_t* client;
	client = TCP_CreateClient(serverIP, serverPort);
	if (!client)
	{
		printf("\nERROR. coud not connect to server ip %s port %d. \nNothing to do. user should re Run.\n\n", serverIP, serverPort);
		return 1;
	}

	int sent_bytes;
	int recv_bytes;
	while ( TRUE )
	{
		printf("\nType msg to be send (0000 to exit): ");

		fgets(msg, MAX_MSG_SIZE , stdin); 		/* safer. no overflow */
		msg[strcspn(msg, "\n")] = 0; 			/* remove trailing \n */
		sanity_check(msg, MAX_MSG_SIZE, '_'); 	/* remove illegal unsafe char */

		if (! strcmp("0000" , msg)) /* breaking out */
		{
			break;
		}

		sent_bytes = TCP_ClientSend(client, msg, strlen(msg) + 1 );
		if (0 > sent_bytes)
		{
			printf("\nError. send %d bytes", sent_bytes);
		}
		else
		{
			recv_bytes = TCP_ClientRecive(client, buffer, MAX_MSG_SIZE);
			if (0 == recv_bytes)
			{
				printf("\n server closed connection. exiting.");
				break;
			}
			printf("recived(%d): %s.", recv_bytes, (char*) buffer);

		}
	}

	TCP_DestroyClient(client);
	printf("\n--END--\n");
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
