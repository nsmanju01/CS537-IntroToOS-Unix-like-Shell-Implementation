all : mysh

mysh : mysh.o
	gcc -o mysh mysh.o 

mysh.o : mysh.c
	gcc -c mysh.c

clean :
	rm mysh *.o
