CC = gcc
CFLAGS = -Wall -O3 -ffast-math
LDLIBS = -lm

cube:
	$(CC) $(CFLAGS) -o cube spinning-cube.c $(LDLIBS)


clean:
	rm -f cube
