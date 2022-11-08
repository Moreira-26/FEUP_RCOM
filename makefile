#This is a makefile

all: proj

proj: receiver.c sender.c
	gcc -o receiver receiver.c
	gcc -o sender sender.c

clean:
	rm -rf receiver
	rm -rf sender
	rm -rf *.exe
	rm -rf *.o
