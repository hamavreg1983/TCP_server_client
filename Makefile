# Yuval Hamberg
# yuval.hamberg@gmail.com
# 04/07/2017
# TCP

# File names
EXE_NAME1 = SERVERapp
EXE_NAME2 = userinputClient
EXE_NAME3 = autoinputClient
#SOURCES = $(wildcard *.cpp)
#OBJECTS = $(SOURCES:.cpp=.o)
#H_FILES = $(wildcard *.h)

NEEDED_LIB = list/build/liblist.a

CC = gcc
CFLAGS = -g -Wall -pedantic -Isrc/ -Ilist/src

.Phony : clean rebuild run

# Main target
$(EXE_NAME1): src/tcp.o server/server.o $(NEEDED_LIB)
	$(CC) $(CFLAGS) src/tcp.o server/server.o $(NEEDED_LIB) -o $(EXE_NAME1) 

$(EXE_NAME2): client_test/client_userInput.o src/tcp_client.o  $(NEEDED_LIB)
	$(CC) $(CFLAGS) client_test/client_userInput.o src/tcp_client.o  $(NEEDED_LIB) -o $(EXE_NAME2)
 
$(EXE_NAME3): client_test/client_autoTest.o src/tcp.o src/tcp_client.o $(NEEDED_LIB)
	$(CC) $(CFLAGS) client_test/client_autoTest.o src/tcp.o src/tcp_client.o $(NEEDED_LIB) -o $(EXE_NAME3) 
 
# To obtain object files
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(NEEDED_LIB) :
	$(MAKE) all -C list

clean:
	rm -f *.o src/*.o client_test/*.o server/*.o
	rm -f *~
	rm -f $(EXE_NAME1) $(EXE_NAME2) $(EXE_NAME3)
	rm -f a.out
	$(MAKE) clean -C list

rebuild : clean $(EXE_NAME1) $(EXE_NAME2)


