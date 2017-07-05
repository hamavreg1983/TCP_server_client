/*
 * tcp.h
 *
 *  Created on: Jul 4, 2017
 *      Author: Yuval Hamberg
 */

#ifndef TCP_H_
#define TCP_H_

typedef unsigned int uint;
typedef int bool;
#define TRUE 1
#define FALSE 0

typedef bool (*actionFunc)(void* _data, size_t _sizeData, void* _contex);

typedef struct TCP_S TCP_S_t;

TCP_S_t* TCP_CreateServer(uint _port);
void TCP_DestroyServer(TCP_S_t* _TCP);

bool TCP_ServerConnect(TCP_S_t* _TCP);
bool TCP_ServerDisconnect(TCP_S_t* _TCP, uint _socketNum);

int TCP_DoServer(TCP_S_t* _TCP, actionFunc _appFunc);


/* for use of server and client */
int TCP_Send(TCP_S_t* _TCP, uint _socketNum, void* _msg, uint _msgLength);
int TCP_Recive(TCP_S_t* _TCP, uint _socketNum, void* _buffer, uint _bufferMaxLength);



TCP_S_t* TCP_CreateClient(char* _ServerIP, uint _serverPort);
void TCP_DestroyClient(TCP_S_t* _TCP);


/* no implemention for ClientDisconnect */

int TCP_ClientGetSocket(TCP_S_t* _TCP);


#endif /* TCP_H_ */


