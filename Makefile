PROJS=smore
CC=gcc
CFLAGS=-m64 -Wall -Wextra -pedantic -ansi
CFILE=smore.c
all : $(PROJS)
	@echo Done!

$(PROJS): $(CFILE)
	$(CC) $(CFLAGS) -o $@ $(@:=.c)
