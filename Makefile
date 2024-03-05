CC = gcc
CFLAGS = -Wall -g
INCS = -I/usr/X11R6/include -I/usr/include/freetype2
LIBS = -L/usr/X11R6/lib
CLIBS = -lXft -lX11

all: tibaji

tibaji: tibaji.c config.h
	$(CC) $(CFLAGS) $(INCS) $(LIBS) -o $@ tibaji.c $(CLIBS)

clean:
	rm tibaji
