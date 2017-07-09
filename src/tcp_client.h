/**
 * @author Yuval Hamberg
 * @date Jul 4, 2017
 *
 * @brief A TCP client ADT for future projects use.
 *
 * @bug after awile all sockets are full and no thramistion is able to send
 */

#ifndef TCP_CLIENT_H_
#define TCP_CLIENT_H_

typedef unsigned int uint;
typedef int bool;
#define TRUE 1
#define FALSE 0

/* ~~~ Struct ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
typedef struct TCP_C TCP_C_t;


/**
 * @brief Create all is needed for a TCP client to connect to the server
 * @param _ServerIP the ip address (ipv4 or ipv6) of the server
 * @param _serverPort the listning for new connection port on the server
 * @return pointer to the newly create struct
 */
TCP_C_t* TCP_CreateClient(char* _ServerIP, uint _serverPort);


/**
 * @brief Cleans up and free after the program. This include the dissconect function inseide it.
 * @param _TCP pointer to the struct
 * @return void. silent fail.
 */
void TCP_DestroyClient(TCP_C_t* _TCP);


/**
 * @brief Function to send data (back?) to a client.
 * @param _TCP a pointer to the TCP struct
 * @param _msg the data to be send. up to BUFFER_MAX_SIZE bytes.
 * @param _msgLength the data send size.
 * @return positive number represent the number of bytes send. negative number represent error.
 */
int TCP_ClientSend(TCP_C_t* _TCP, void* _msg, uint _msgLength);

/**
 * @brief Function to invoke data read from clients without looping. just a singal read.
 * @param _TCP a pointer to the TCP struct
 * @param _buffer the data buffer to be read to. up to BUFFER_MAX_SIZE bytes.
 * @param _bufferMaxLength the data buffer max size.
 * @return positive number represent the number of bytes read. negative number represent error.
 */
int TCP_ClientRecive(TCP_C_t* _TCP, void* _buffer, uint _bufferMaxLength);

#endif /* TCP_CLIENT_H_ */
