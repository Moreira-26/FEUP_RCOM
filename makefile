CC=gcc
CFLAGS=-Wall
DEPS = sender.h receiver.h 
OBJ = sender.o receiver.o

%.o: $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< 

app: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ 

.PHONY: clean

clean: 
	rm -f ./*.o
	rm -f ./app