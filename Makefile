CC=gcc
LDFLAGS=-lcomdlg32 #-static-libgcc
#CFLAGS=-m32 -mwindows

.PHONY:
	all clean
all:
	$(CC) -o DLLInjection.exe DLLInjection.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f DLLInjection.exe
