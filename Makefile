CFLAGS=-Wall -Werror -O2

all: pipesig

pipesig: pipesig.o
	$(CC) -o pipesig pipesig.o

clean:
	rm -f pipesig *.o
