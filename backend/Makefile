# Smart Deadlock Detection and Prevention System - Backend Build
# Compile with: make (on systems with make) or use build.bat on Windows

CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = deadlock_system
SOURCES = main.c process.c resource.c deadlock.c
HEADERS = process.h resource.h deadlock.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	del /f $(TARGET).exe 2>nul || rm -f $(TARGET)

.PHONY: all clean
