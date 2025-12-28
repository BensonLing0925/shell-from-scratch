CC = gcc

CFLAGS   = -Wall -Wextra -O0 -g
CPPFLAGS =

ifeq ($(DEBUG),1)
CPPFLAGS += -DARENA_DEBUG
else
CPPFLAGS += -DNDEBUG
endif

TARGET = arena_test
SRCS   = arena.c main_arena.c
OBJS   = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
