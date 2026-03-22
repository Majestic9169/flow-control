CC := gcc
CFLAGS := -Wall -g -ggdb \
					-Wunreachable-code \
					-Wextra -Werror -Wshadow -pedantic \
					-Wfloat-equal \
					-O2 -I../lib/

TESTFILE := testfile.txt

.PHONY: all clean run

all: user1 user2

user1: ./user1.c ../lib/libksocket.a
	${CC} ${CFLAGS} $< -o $@ -lksocket -L../lib/

user2: ./user2.c ../lib/libksocket.a
	${CC} ${CFLAGS} $< -o $@ -lksocket -L../lib/

run: all
	./user2 9001 127.0.0.1 9000 received.bin 2>user2.log &
	./user1 9000 127.0.0.1 9001 ${TESTFILE}
	kill %1 2>/dev/null; true

clean:
	-rm user1 user2 received.bin