TUSB_DIR = ../..
OBJS = \
	hello.rel \
	hello_descript.rel \
	$(TUSB_DIR)/src/usb_driver.rel \
	$(TUSB_DIR)/src/uart_driver.rel \
	$(TUSB_DIR)/src/util.rel \
	$(TUSB_DIR)/src/i2c_driver.rel

BINS = hello hello_test

CFLAGS = -I $(TUSB_DIR)/include
LDFLAGS = --xram-loc 0xF800 --xram-size 0x500

all: $(BINS)

hello_test:
	gcc -o hello_test hello_test.c -lusb

hello: hello.ihx
	objcopy -I ihex -O binary hello.ihx hello

install: hello
	$(TUSB_DIR)/bootload/bootload hello

hello.ihx: $(OBJS)
	sdcc -o hello.ihx $(OBJS) $(LDFLAGS)

%.rel: %.c $(TUSB_DIR)/include/*.h
	sdcc -o $@ -c $< $(CFLAGS)

clean:
	rm -f *.ihx *.lnk *.lst *.map *.mem *.rst *.sym *.asm
	rm -f $(BINS)
	rm -f $(OBJS)
