CC      = gcc
CFLAGS  = -g -Wall -O2 -Wno-unused-value
DEFS    =
INCDIR  = ./
OBJ     = mkbench.o proc.o

all: mkbench

mkbench: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

clean:
	rm -rf *.o *~ mkbench \#*

.c.o:
	$(CC) -c $(CFLAGS) $(DEFS) -I$(INCDIR) $<
