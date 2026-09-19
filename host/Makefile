OBJS = pdm_capture.o
LIBS = -lusb-1.0
CFLAGS = -Wall -O2

all: pdm_capture unpack_bits

pdm_capture: $(OBJS)
	$(CC) $(CFLAGS) -o pdm_capture $(OBJS) $(LIBS)

unpack_bits: unpack_bits.o
	$(CC) $(CFLAGS) -o unpack_bits unpack_bits.c

clean:
	rm -f pdm_capture unpack_bits $(OBJS)
