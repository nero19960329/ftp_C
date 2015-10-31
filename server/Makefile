all: server

server: server.o
	gcc -Wall server.o -o server -lpthread

server.o: server.c
	gcc -c -Wall server.c -lpthread
	
clean:
	rm -rf *.o server *~
