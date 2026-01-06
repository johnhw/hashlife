CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic 
CFLAGS_DEBUG = -g -DDEBUG  -fstack-protector-strong -fno-omit-frame-pointer
CFLAGS_OPT = -O4 -DNDEBUG
CFLAGS += $(CFLAGS_OPT)
TARGET = test_hashlife
OBJS = test_hashlife.o hashlife.o cell_io.o timeit.o

MIN_SRCS = min_hashlife.c cell_io.c main.c
MIN_OBJS = $(MIN_SRCS:.c=.o)

.PHONY: all clean

test: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

test_hashlife.o: test_hashlife.c
	$(CC) $(CFLAGS) -c test_hashlife.c

hashlife.o: hashlife.c
	$(CC) $(CFLAGS) -c hashlife.c

cell_io.o: cell_io.c
	$(CC) $(CFLAGS) -c cell_io.c
	
timeit.o: timeit.c
	$(CC) $(CFLAGS) -c timeit.c

min: $(MIN_OBJS)
	$(CC) $(CFLAGS_DEBUG) -o min_hashlife $(MIN_OBJS)	

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

