CC = gcc
CFLAGS = -g -Wall
TARGET = test_hashlife
OBJS = test_hashlife.o hashlife.o coords.o

.PHONY: all clean

test: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

test_hashlife.o: test_hashlife.c
	$(CC) $(CFLAGS) -c test_hashlife.c

hashlife.o: hashlife.c
	$(CC) $(CFLAGS) -c hashlife.c

coords.o: coords.c
	$(CC) $(CFLAGS) -c coords.c

clean:
	rm -f $(TARGET) $(OBJS)