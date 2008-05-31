VERSION=0.1
SRCS=undbx.c dbxsys.c dbxread.c
HDRS=dbxsys.h dbxread.h
CPPFLAGS+=-DDBX_VERSION=\"$(VERSION)\"
CFLAGS+=-Wall -O3 
CFLAGS_LINUX=-g
CFLAGS_WINDOWS=-s

all: undbx undbx.exe

clean:
	rm -f undbx undbx.exe

undbx: $(SRCS) $(HDRS) Makefile
	gcc -o undbx $(CPPFLAGS) $(CFLAGS) $(CFLAGS_LINUX) $(LDFLAGS) $(SRCS)

undbx.exe: $(SRCS) $(HDRS) Makefile
	i586-mingw32msvc-gcc -o undbx.exe $(CPPFLAGS) $(CFLAGS) $(CFLAGS_WINDOWS) $(LDFLAGS) $(SRCS)