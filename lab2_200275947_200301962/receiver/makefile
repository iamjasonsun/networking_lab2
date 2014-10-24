CC=gcc
CFLAGS=-I.
DEPS = # header file 
OBJ = client.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

client: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf  *.o *~ client