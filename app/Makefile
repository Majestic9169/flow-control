CC := gcc
CFLAGS := -Wall -g -ggdb \
					-Wunreachable-code \
					-Wextra -Werror -Wshadow -pedantic \
					-Wfloat-equal \
					-O2 -I../lib/

TESTFILE := testfile.txt

.PHONY: all clean run

all: sender receiver

sender: ./user1.c ../lib/libksocket.a
	${CC} ${CFLAGS} $< -o $@ -lksocket -L../lib/

receiver: ./user2.c ../lib/libksocket.a
	${CC} ${CFLAGS} $< -o $@ -lksocket -L../lib/

run: all
	./receiver 9001 127.0.0.1 9000 &
	./sender 9000 127.0.0.1 9001 ${TESTFILE}

clean:
	-@rm user1 user2 received.bin sender receiver
