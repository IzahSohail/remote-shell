# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Werror -g -Iinclude
TARGET = myshell

# Directories
SRC_DIR = src
OBJ_DIR = obj
INCLUDE_DIR = include

# Source and Object Files
SRCS = $(SRC_DIR)/myshell.c $(SRC_DIR)/parser.c $(SRC_DIR)/executor.c
OBJS = $(OBJ_DIR)/myshell.o $(OBJ_DIR)/parser.o $(OBJ_DIR)/executor.o

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

# Compile myshell.c
$(OBJ_DIR)/myshell.o: $(SRC_DIR)/myshell.c $(INCLUDE_DIR)/parser.h $(INCLUDE_DIR)/executor.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/myshell.c -o $(OBJ_DIR)/myshell.o

# Compile parser.c
$(OBJ_DIR)/parser.o: $(SRC_DIR)/parser.c $(INCLUDE_DIR)/parser.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/parser.c -o $(OBJ_DIR)/parser.o

# Compile executor.c
$(OBJ_DIR)/executor.o: $(SRC_DIR)/executor.c $(INCLUDE_DIR)/executor.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/executor.c -o $(OBJ_DIR)/executor.o

# Create object directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean build files
clean:
	rm -rf $(TARGET) $(OBJ_DIR)

.PHONY: all clean
