.PHONY : all clean

all : msg proxy

msg : msg.c
	gcc -o msg msg.c -O3

proxy : proxy.c
	gcc -o proxy proxy.c -O3 -levent

clean :
	rm -f proxy msg
