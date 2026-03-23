CC := gcc
CFLAGS := -Wall -g -ggdb \
					-Wunreachable-code \
					-Wextra -Werror -Wshadow -pedantic \
					-Wfloat-equal \
					-O2 -I../lib/

TESTFILE := ../payload.png

.PHONY: all clean run

all: sender receiver

sender: ./user1.c ../lib/libksocket.a
	${CC} ${CFLAGS} $< -o $@ -lksocket -L../lib/

receiver: ./user2.c ../lib/libksocket.a
	${CC} ${CFLAGS} $< -o $@ -lksocket -L../lib/

run: all
	./receiver 9001 127.0.0.1 9000 received.bin &
	./sender 9000 127.0.0.1 9001 ${TESTFILE}

manyrun: all
	./receiver 9001 127.0.0.1 9000 received_1.bin &
	./sender 9000 127.0.0.1 9001 ${TESTFILE} &
	./receiver 9003 127.0.0.1 9002 received_2.bin &
	./sender 9002 127.0.0.1 9003 ${TESTFILE} 

clean:
	-@rm user1 user2 received.bin sender receiver *.bin
