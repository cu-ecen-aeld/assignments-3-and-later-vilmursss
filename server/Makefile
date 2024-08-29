# Variables
TARGET = aesdsocket
SRC = server.c
OBJ = $(SRC:.c=.o)
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -Werror -DUSE_AESD_CHAR_DEVICE=1
LDFLAGS = -lpthread

# Default target
all: $(TARGET)

# Compile the target
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile the object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(TARGET) $(OBJ)

# Phony targets
.PHONY: all clean
