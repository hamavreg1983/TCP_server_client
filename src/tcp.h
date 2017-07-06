/*
 * tcp.h
 *
 *  Created on: Jul 4, 2017
 *      Author: Yuval Hamberg
 */

/* #TODO commnet project and function by doxygen in h files */
/* #TODO divaide server and client. maybe just h file */

#ifndef TCP_H_
#define TCP_H_

#define MAX_CLIENTS_NUM 1000
#define BUFFER_MAX_SIZE 1024

typedef unsigned int uint;
typedef int bool;
#define TRUE 1
#define FALSE 0

/* TODO add deteailes who send the msg */
/* TODO sould be (void* input, inputsize, ? senderInfo, void* output, outputsize, void* contex) */
/* TODO struct socket num, sender info, ptr to data recive, size, and return the same stracut with */
typedef int (*actionFunc)(void* _data, size_t _sizeData, void* _contex);

typedef struct TCP_S TCP_S_t;

/* add max connection number */
/* add serverIP if more than one iterface exsist */
/* move user action func here */
TCP_S_t* TCP_CreateServer(uint _port);
void TCP_DestroyServer(TCP_S_t* _TCP);

bool TCP_ServerConnect(TCP_S_t* _TCP);
bool TCP_ServerDisconnect(TCP_S_t* _TCP, uint _socketNum);

/* TODO remove actionfunc from here to create */
/* TODO remove the send from doServer, and the app decide when to send data. for exmaple after all package msg and not after fragments */
/* TODO change name to RunServer */
/* TODO do not accpet NULL as app func */
bool TCP_DoServer(TCP_S_t* _TCP, actionFunc _appFunc);


/* for use of server and client */
int TCP_Send(TCP_S_t* _TCP, uint _socketNum, void* _msg, uint _msgLength);
int TCP_Recive(TCP_S_t* _TCP, uint _socketNum, void* _buffer, uint _bufferMaxLength);

/* TODO user function when client is accepted to server */
/* TODO user function when client is accepted try to connect but rejected beacuse too many connection. or just have one callback of all kind of errors */
/* TODO user function when client disconnected or was disconnected */

TCP_S_t* TCP_CreateClient(char* _ServerIP, uint _serverPort);
void TCP_DestroyClient(TCP_S_t* _TCP);


/* no implemention for ClientDisconnect */
int TCP_ClientGetSocket(TCP_S_t* _TCP);


#endif /* TCP_H_ */


