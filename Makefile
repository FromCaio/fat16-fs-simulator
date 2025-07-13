CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SRC = src
BIN = bin

SRCS = $(SRC)/shell.c $(SRC)/fat_fs.c
OBJS = $(SRCS:.c=.o)

all: $(BIN)/shell

$(BIN)/shell: $(SRCS)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -rf $(BIN)