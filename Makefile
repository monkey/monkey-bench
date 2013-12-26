CC      = gcc
CFLAGS  = -g -Wall
DEFS    =
INCDIR  = ./
OBJ     = mkbench.o proc.o

all: mkbench

mkbench: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

clean:
	rm -rf *.o *~ wr \#*

.c.o:
	$(CC) -c $(CFLAGS) $(DEFS) -I$(INCDIR) $<
