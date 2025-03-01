CC = gcc
CFLAGS = -Wall -Werror -g
TARGET = myshell

all: $(TARGET)

$(TARGET): myshell.c
	$(CC) $(CFLAGS) -o $(TARGET) myshell.c

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean