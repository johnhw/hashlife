CC = gcc
CFLAGS = -g -Wall
TARGET = test_alt_hashlife
OBJS = test_alt_hashlife.o alt_hashlife.o

.PHONY: all clean

test: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

test_hashlife.o: test_alt_hashlife.c
	$(CC) $(CFLAGS) -c test_alt_hashlife.c

hashlife.o: alt_hashlife.c
	$(CC) $(CFLAGS) -c alt_hashlife.c

clean:
	rm -f $(TARGET) $(OBJS)