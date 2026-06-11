CC = gcc
CFLAGS = $(shell pkg-config --cflags raylib gl) -Wall -Wextra
LIBS = $(shell pkg-config --libs raylib gl) -lm

blackhole: main.c vec3.c blackhole.c ray.c
	$(CC) main.c vec3.c blackhole.c ray.c $(CFLAGS) $(LIBS) -o blackhole

clean:
	rm -f blackhole
