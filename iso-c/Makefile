CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic 
CFLAGS_DEBUG = -g
CFLAGS_OPT = -O4 -DNDEBUG
CFLAGS += $(CFLAGS_DEBUG)

.PHONY: all clean

all: hashlife  # Set default target to hashlife


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


hashlife: main.o hashlife.o cell_io.o timeit.o
	$(CC) $(CFLAGS) -o hashlife main.o hashlife.o cell_io.o timeit.o

test: test_hashlife.o hashlife.o cell_io.o timeit.o
	$(CC) $(CFLAGS) -o test_hashlife test_hashlife.o hashlife.o cell_io.o timeit.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

