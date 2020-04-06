TARGET1 = 	oss
TARGET2 =	user

OBJS1   =	ossim.o
OBJS2   =	user_process.o

all:	oss user

$(TARGET1):	$(OBJS1)
	$(CC) -o $(TARGET1) $(OBJS1) -lpthread

$(TARGET2):	$(OBJS2)
	$(CC) -o $(TARGET2) $(OBJS2) -lpthread

$(OBJS1):	ossim.c
	$(CC) $(CFLAGS) -c ossim.c

$(OBJS2):	user_process.c
	$(CC) $(CFLAGS) -c user_process.c

clean:
	/bin/rm -f *.o $(TARGET1) $(TARGET2)


