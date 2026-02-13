CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pedantic
TARGET = binagent
SRC = src/main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
