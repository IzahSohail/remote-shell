# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Werror -g -Iinclude
SERVER_TARGET = server
CLIENT_TARGET = myshell  # Since client is in myshell.c

# Directories
SRC_DIR = src
OBJ_DIR = obj
INCLUDE_DIR = include

# Source and Object Files
SERVER_SRCS = $(SRC_DIR)/server.c $(SRC_DIR)/executor.c $(SRC_DIR)/parser.c
CLIENT_SRCS = $(SRC_DIR)/myshell.c
SERVER_OBJS = $(OBJ_DIR)/server.o $(OBJ_DIR)/executor.o $(OBJ_DIR)/parser.o
CLIENT_OBJS = $(OBJ_DIR)/myshell.o

# Default target
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Build the server executable
$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) $(CFLAGS) $(SERVER_OBJS) -o $(SERVER_TARGET) -lpthread    # 💡 Added -lpthread here


# Build the client executable (myshell)
$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $(CLIENT_OBJS) -o $(CLIENT_TARGET)

# Compile server.c
$(OBJ_DIR)/server.o: $(SRC_DIR)/server.c $(INCLUDE_DIR)/executor.h $(INCLUDE_DIR)/parser.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/server.c -o $(OBJ_DIR)/server.o

# Compile myshell.c (Client)
$(OBJ_DIR)/myshell.o: $(SRC_DIR)/myshell.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/myshell.c -o $(OBJ_DIR)/myshell.o

# Compile executor.c
$(OBJ_DIR)/executor.o: $(SRC_DIR)/executor.c $(INCLUDE_DIR)/executor.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/executor.c -o $(OBJ_DIR)/executor.o

# Compile parser.c
$(OBJ_DIR)/parser.o: $(SRC_DIR)/parser.c $(INCLUDE_DIR)/parser.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/parser.c -o $(OBJ_DIR)/parser.o

# Create object directory if it doesn't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Clean build files
clean:
	rm -rf $(SERVER_TARGET) $(CLIENT_TARGET) $(OBJ_DIR)

.PHONY: all clean
