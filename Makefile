CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L -I./include -I./src
LDFLAGS = -lpthread

SRC_DIR  = src
OBJ_DIR  = obj
TEST_DIR = tests

SRCS = $(SRC_DIR)/mr.c         \
       $(SRC_DIR)/mapper.c     \
       $(SRC_DIR)/reducer.c    \
       $(SRC_DIR)/hashtable.c  \
       $(SRC_DIR)/coda.c       \
       $(SRC_DIR)/io.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

LIB      = libmr.a
TEST_BIN = test_ping

.PHONY: all test clean

all: $(LIB)

test: $(TEST_BIN)

$(TEST_BIN): $(TEST_DIR)/test_ping.c $(LIB)
	$(CC) $(CFLAGS) $< -L. -lmr $(LDFLAGS) -o $@

$(LIB): $(OBJS)
	ar rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -f $(OBJ_DIR)/*.o $(LIB) $(TEST_BIN)
