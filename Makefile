CC = cc
CFLAGS = -std=gnu11 -Wall -Wextra
TARGET = banker

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET)

clean:
	rm -f $(TARGET)