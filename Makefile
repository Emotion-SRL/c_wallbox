STAGING_DIR=/home/builder/source/staging_dir
export STAGING_DIR

COMPILER=$(STAGING_DIR)/toolchain-mipsel_24kc_gcc-7.3.0_musl/bin/mipsel-openwrt-linux-gcc
TARGET_DIR=$(STAGING_DIR)/target-mipsel_24kc_musl/usr

CFLAGS=-I/home/builder/libwebsockets/include -I$(TARGET_DIR)/include
LDFLAGS=-L/home/builder/libwebsockets/build/lib -L$(TARGET_DIR)/lib -Wl,-rpath-link,$(TARGET_DIR)/lib
STATIC_LIBS=-l:libwebsockets.a -l:libssl.a -l:libcrypto.a
LIBS=-lwebsockets -lssl -lcrypto
SRC=src/main.c src/utils.c src/serial.c
COMPFLAGS=-Wall

ws_client: $(SRC)
	$(COMPILER) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(STATIC_LIBS) $(COMPFLAGS) -g

release: $(SRC)
	$(COMPILER) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(STATIC_LIBS) $(COMPFLAGS) -O3

wsc_dyn: $(SRC)
	$(COMPILER) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS) $(COMPFLAGS)

build_x86: $(SRC)
	gcc -o $@.out $^ $(LIBS) $(COMPFLAGS) -fsanitize=address -g

clear:
	rm ws_client wsc_dyn build_x86.out


