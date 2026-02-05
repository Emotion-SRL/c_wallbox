STAGING_DIR=/home/builder/source/staging_dir
export STAGING_DIR

COMPILER=$(STAGING_DIR)/toolchain-mipsel_24kc_gcc-7.3.0_musl/bin/mipsel-openwrt-linux-gcc
TARGET_DIR=$(STAGING_DIR)/target-mipsel_24kc_musl/usr

CFLAGS=-I/home/builder/libwebsockets/include -I$(TARGET_DIR)/include -I$(TARGET_DIR)/include/json-c/
LDFLAGS=-L/home/builder/libwebsockets/build/lib -L$(TARGET_DIR)/lib -Wl,-rpath-link,$(TARGET_DIR)/lib
STATIC_LIBS=-l:libwebsockets.a -l:libssl.a -l:libcrypto.a -l:libjson-c.a
LIBS=-lwebsockets -lssl -lcrypto -ljson-c
SRC=src/main.c src/utils.c src/serial.c
CLI_SRC=src/onion_cli/onion_cli.c src/serial.c src/utils.c
COMPFLAGS=-Wall #-Wextra #-Wpedantic

#stupid shit for stupid raycaster
RAY_SRC=src/ray/main.c
#end of stupid


ws_client: $(SRC) $(CLI_SRC)
	$(COMPILER) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS) $(STATIC_LIBS) $(COMPFLAGS) -g
	$(COMPILER) $(CFLAGS) -o onion_cli $(CLI_SRC) $(LDFLAGS) -ljson-c

release: $(SRC)
	$(COMPILER) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(STATIC_LIBS) $(COMPFLAGS) -O3

wsc_dyn: $(SRC)
	$(COMPILER) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS) $(COMPFLAGS)

build_x86: $(SRC) $(CLI_SRC)
	gcc -o $@.out $(SRC) $(LIBS) $(COMPFLAGS) -I/usr/include/json-c/ -fsanitize=address -g
	gcc -o onion_cli $(CLI_SRC) -I/usr/include/json-c/ -ljson-c -fsanitize=address

clear:
	rm ws_client wsc_dyn build_x86.out

ray: $(RAY_SRC)
	$(COMPILER) $(CFLAGS) -o ray $(RAY_SRC) $(LDFLAGS) -lncurses

