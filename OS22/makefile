CC = gcc
CFLAGS = -Wall -g `pkg-config --cflags gtk4`
LDFLAGS = `pkg-config --libs gtk4`
SRC = main.c gui.c queue.c pcb.c mutex.c instruction.c
OBJ = $(SRC:.c=.o)
EXEC = scheduler

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC) $(LDFLAGS)

main.o: main.c main.h gui.h queue.h pcb.h mutex.h instruction.h
	$(CC) $(CFLAGS) -c main.c -o main.o

gui.o: gui.c main.h gui.h queue.h pcb.h mutex.h
	$(CC) $(CFLAGS) -c gui.c -o gui.o

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) -c queue.c -o queue.o

pcb.o: pcb.c pcb.h
	$(CC) $(CFLAGS) -c pcb.c -o pcb.o

mutex.o: mutex.c mutex.h queue.h
	$(CC) $(CFLAGS) -c mutex.c -o mutex.o

instruction.o: instruction.c instruction.h
	$(CC) $(CFLAGS) -c instruction.c -o instruction.o

clean:
	rm -f $(OBJ) $(EXEC)