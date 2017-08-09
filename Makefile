CFLAGS=-I/usr/local/include
LDFLAGS=-L/usr/local/lib
LIBS=-lfreeimage -largp

all:
	gcc $(CFLAGS) main.c cmdline.c output.c -o anim1b $(LDFLAGS) $(LIBS)
