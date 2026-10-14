CC     = gcc
CFLAGS = -std=c17 -Wall -Wextra -g

app: main.o geometry.o
	$(CC) -o app main.o geometry.o -lm

%.o: %.c geometry.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f app *.o
