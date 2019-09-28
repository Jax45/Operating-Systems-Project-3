CC		= gcc
CFLAGS		= -g
TARGET		= oss
OBJECTS		= oss.o
CHILD		= child
CHILDOBJS	= child.o

.SUFFIXES: .c .o

ALL: oss child

$(TARGET): $(OBJECTS)
	$(CC) -Wall -o $@ $(OBJECTS)

$(CHILD): $(CHILDOBJS)
	$(CC) -Wall -o $@ child.o

.c.o: oss.c child.c
	$(CC) -Wall $(CFLAGS) -c $<

.PHONY: clean
clean:
	/bin/rm -f *.o $(TARGET) $(CHILD)
