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

typedef struct TCP TCP_t;

TCP_t* TCP_CreateServer(uint _port);
TCP_t* TCP_CreateClient(char* _ServerIP, uint _serverPort);
void TCP_DestroyServer(TCP_t* _TCP);
void TCP_DestroyClient(TCP_t* _TCP);

bool TCP_ServerConnect(TCP_t* _TCP);
bool TCP_ClientConnect(TCP_t* _TCP);
bool TCP_ServerDisconnect(TCP_t* _TCP, uint _socketNum);

int TCP_DoServer(TCP_t* _TCP, actionFunc _appFunc);
int TCP_Send(TCP_t* _TCP, uint _socketNum, void* _msg, uint _msgLength);
int TCP_Recive(TCP_t* _TCP, uint _socketNum, void* _buffer, uint _bufferMaxLength);

int TCP_ClientGetSocket(TCP_t* _TCP);

#endif /* TCP_H_ */


