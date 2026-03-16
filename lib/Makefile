CC := gcc
CFLAGS := -Wall -g -ggdb \
					-Wunreachable-code \
					-Wextra -Werror -Wshadow -pedantic \
					-Wfloat-equal -pthread \
					-O2 

all: libksocket.a

libksocket.a: ksocket.o
	ar rvs $@ $<

ksocket.o: ./ksocket.c ./ksocket.h ./kinternal.h
	${CC} ${CFLAGS} -c -o $@ $<

clean: 
	rm *.o libksocket.a
