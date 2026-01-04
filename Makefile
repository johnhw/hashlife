CC = gcc
CFLAGS = -O4 -Wall -Wextra -std=c11 -pedantic -DNDEBUG
TARGET = test_hashlife
OBJS = test_hashlife.o hashlife.o cell_io.o timeit.o

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

clean:
	rm -f $(TARGET) $(OBJS)