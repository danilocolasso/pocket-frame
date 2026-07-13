TARGET := App/PocketFrame/pocket-frame
CROSS_COMPILE ?= arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
SYSROOT := $(shell $(CC) --print-sysroot)

CFLAGS := -Os -Wall -I$(SYSROOT)/usr/include/SDL
LDFLAGS := -lSDL -lSDL_image -lSDL_ttf

$(TARGET): src/main.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

setup:
	@./setup.sh

clean:
	rm -f $(TARGET)

# build inside the toolchain container (from the project root):
#   docker run --rm --platform linux/amd64 -v "$$PWD":/root/workspace \
#     aemiii91/miyoomini-toolchain:latest bash -c '. /root/setup-env.sh && make'
.PHONY: clean setup
