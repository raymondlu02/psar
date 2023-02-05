CC     = gcc
CFLAGS = -Wextra -pthread

master:
	$(CC) $(CFLAGS) master_socketThread.c master.c -o master -lm
slave:
	$(CC) $(CFLAGS) slaveFonc.c test_general.c  -o slave -lm

all: master slave

clean:
	rm -f master 
	rm -f slave
