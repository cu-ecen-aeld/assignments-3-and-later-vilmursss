# Variables
TARGET = writer
SRC = writer.c
OBJ = $(SRC:.c=.o)
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra -Werror

# Default target
all: $(TARGET)

# Compile the target
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compile the object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(TARGET) $(OBJ)

# Phony targets
.PHONY: all clean
