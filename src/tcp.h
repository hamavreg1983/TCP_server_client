/**
 * @author Yuval Hamberg
 * @date Jul 4, 2017
 *
 * @brief A TCP server and client ADT for future projects use.
 *
 * @bug not a possible TCP_SERVER_USER_ERROR were included./\
 * 		when calling stop Server function and server is waiting on select nothing would happen until next transmistion would arrive
 * 		signal causes coreDump. a fix is explained here, but is not implemented. https://stackoverflow.com/questions/6962150/catching-signals-while-reading-from-pipe-with-select
 */

#ifndef TCP_H_
#define TCP_H_

/* TODO add timeout to connected client. list populated by use. each one has time stamp when to die. before each select chech in loop if last node need to die  */

/* ~~~ Defines ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

#define MAX_CLIENTS_NUM 1000
#define BUFFER_MAX_SIZE 1024

typedef unsigned int uint;
typedef int bool;
#define TRUE 1
#define FALSE 0

typedef enum TCP_SERVER_USER_ERROR {
	TOO_MANY_CONNECTION = 1
} TCP_SERVER_USER_ERROR;

typedef int (*userActionFunc)(void* _data, size_t _sizeData, uint _socketNum, void* _contex);
typedef int (*clientConnectionChangeFunc)(uint _socketNum, void* _contex);
typedef int (*errorFunc)(TCP_SERVER_USER_ERROR _status, uint _socketNum, void* _contex);

/* ~~~ Struct ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
typedef struct TCP_S TCP_S_t;

/* ~~~ API function ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/**
 * @brief Create the server and setup all is needed of work
 * @param _port the server listing port for new client connections.
 * @param _serverIP The server IP address in case of a few interfaces for the same computer. Can be left NULL for defualt ip selected.
 * @param _maxConnections if more connection than this number are simultansly try to connect, clients would be dealt and probably droped.
 * @param _reciveDataFunc user function to invoke when data is recived at server
 * @param _newClientConnected user function to invoke when new client is connected. can be left NULL.
 * @param _clientDissconected user function to invoke when client is disconnected, either because of server of client induces. can be left NULL.
 * @param _errorFunc user function to invoke when errors occur in server. can be left NULL.
 * @return a pointer to the struct. NULL if failed.
 */
TCP_S_t* TCP_CreateServer(uint _port, const char* _serverIP, uint _maxConnections, uint _timeoutMS,
						userActionFunc _reciveDataFunc,
						clientConnectionChangeFunc _newClientConnected,
						clientConnectionChangeFunc _clientDissconected,
						errorFunc _errorFunc
						);
/**
 * @brief Cleans up and free after the program.
 * @param _TCP pointer to the struct
 * @return void. silent fail.
 */
void TCP_DestroyServer(TCP_S_t* _TCP);

/**
 * @brief start the server. it will loop around waiting for information to arrive or new connection request. When such occures, the user's userActionFunc _reciveDataFunc is actived. Iternal loop until ServerStop is called.
 * @param _TCP pointer to the struct
 * @return the status of return. TRUE when stopped normally or FALSE when failed to run.
 */
bool TCP_RunServer(TCP_S_t* _TCP);

/**
 * @brief stop the TCP_RunServer loop.
 * @param _TCP a pointer to the TCP server struct
 * @return bool TRUE 1 is success or FALSE 0 if failed.
 */
bool TCP_StopServer(TCP_S_t* _TCP);


/**
 * @brief Function to send data (back?) to a client.
 * @param _socketNum a number representing the client the information would be send to.
 * @param _msg the data to be send. up to BUFFER_MAX_SIZE bytes.
 * @param _msgLength the data send size.
 * @return positive number represent the number of bytes send. negative number represent error.
 */
int TCP_Send(uint _socketNum, void* _msg, uint _msgLength);

/**
 * @brief Function to invoke data read from clients without looping. just a singal read.
 * @param _socketNum a number representing the client the information would be read from.
 * @param _buffer the data buffer to be read to. up to BUFFER_MAX_SIZE bytes.
 * @param _bufferMaxLength the data buffer max size.
 * @return positive number represent the number of bytes read. negative number represent error.
 */
int TCP_Recive(uint _socketNum, void* _buffer, uint _bufferMaxLength);






#endif /* TCP_H_ */


