CC	= gcc
CFLAGS  = -g
TARGET  = sum
OBJS    = fork_subtask.o

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) -lm

.PHONY: clean
clean:
	/bin/rm -f *.o $(TARGET)

