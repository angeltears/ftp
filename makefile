.PHONY:clean
CC = g++
CFLAGS = -Wall -g
BIN = miniftpd
OBJS = main.o sysutil.o  str.o ftpproto.o privparent.o session.o parseconf.o  ftpproto.o tunable.o privsock.o
LIBS=-lcrypt

$(BIN):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ -g -lcrypt


%.o:%.cpp
	$(CC) $(CFLAGS) -c $< -o $@ -g -lcrypt
clean:
	rm -f *.o $(BIN)

