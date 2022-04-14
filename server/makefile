__: all

# include paths
HEADERS_INCLUDE_PATH_A = /usr/include

# compiler
CC			= gcc
INCLUDE 	= -I. -I$(HEADERS_INCLUDE_PATH_A)
CDEFINE 	= -DLINUX -DI386 -D_GNU_SOURCE -D__GNU__ -D_POSIX_C_SOURCE -DBACKTRACE_SYMBOLS
CFLAGS		= $(INCLUDE) $(CDEFINE) -g -Wall -Wno-unused-but-set-variable -Wfatal-errors -O0 $(INCLUDE)
# CFLAGS = -O -Wall -ansi # Options for release
LDFLAGS		=
LINKFLAGS	= -o
LIBS		=

# binaryCode
binaryCode += main.c

# binary
binary = server

$(binary):
	$(CC) $(CFLAGS) $(LINKFLAGS) $(binary) $(binaryCode) $(LIBS)

# build
all: clean server

server: $(binary)

clean:
	rm -f *.o
	rm -f *.out
	rm -f *.map
	rm -f $(binary)
