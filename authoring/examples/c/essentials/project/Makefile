CC     = gcc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g

wordfreq: main.o table.o
	$(CC) $(CFLAGS) -o $@ main.o table.o

%.o: %.c table.h
	$(CC) $(CFLAGS) -c $<

# the same program rebuilt with sanitizers: any memory bug becomes a clear report
asan: main.c table.c table.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -o wordfreq-asan main.c table.c

test: wordfreq
	./test.sh

clean:
	rm -f wordfreq wordfreq-asan *.o
