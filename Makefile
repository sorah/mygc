CC = gcc
SRCS = mygc.c
BIN = mygc

all: clean gc

clean:
	rm -f mygc

gc: $(SRCS)
	$(CC) -Wall -Wextra -g -o $(BIN) $(SRCS)

gc_debug:
	$(CC) -Wall -Wextra -g -DDEBUG -o $(BIN) $(SRCS)

test: clean gc_debug
	valgrind ./mygc
