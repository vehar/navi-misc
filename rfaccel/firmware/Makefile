BIN = rfaccel-tx.hex

all: $(BIN)

install: $(BIN)
	pista < pista.conf

%.hex: %.asm
	gpasm $<

clean:
	rm -f *.lst *.cod *.hex