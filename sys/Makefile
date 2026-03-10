CC := gcc
CFLAGS := -Wall -g \
					-Wunreachable-code \
					-Wextra -Werror -Wshadow -pedantic \
					-Wfloat-equal \
					-O2 -I../lib/

.PHONY: all clean run

all: sys_net

run: sys_net
	./$<

sys_net: ./initksocket.c ./initksocket.h ../lib/kinternal.h ../lib/ksocket.h
	${CC} ${CFLAGS} $< -o $@

clean: 
	rm sys_net 
