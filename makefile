.PHONY:clean
CC = g++
CFLAGS = -Wall -g
BIN = miniftpd
OBJS = main.o sysutil.o  str.o ftpproto.o privparent.o session.o parseconf.o  ftpproto.o tunable.o
LIBS=-lcrypt

$(BIN):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ -g


%.o:%.cpp
	$(CC) $(CFLAGS) -c $< -o $@ -g
clean:
	rm -f *.o $(BIN)

