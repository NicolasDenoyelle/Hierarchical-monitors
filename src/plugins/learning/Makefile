CFLAGS=-Wall -Wextra -std=gnu99 -g -I../..
LDFLAGS=-lblas

SRC=perceptron.c utils.c fourier.c
OBJ=$(patsubst %.c, %.o, $(SRC)	)
LIB=learning.monitor_plugin.so

.PHONY: all clean

all: $(LIB)

clean:
	rm -f $(OBJ) $(LIB)

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@ $(LDFLAGS)

$(LIB): $(OBJ)
	$(CC) $(CFLAGS) -fPIC --shared $^ -o $@ $(LDFLAGS)



