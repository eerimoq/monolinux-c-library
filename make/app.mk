BUILD = $(shell readlink -f build)
EXE = $(BUILD)/app
CFLAGS += -fno-omit-frame-pointer
CFLAGS += -Wall -Wextra -std=gnu11
CFLAGS += -g -Og

.PHONY: all run build

all: run

build: $(EXE)

run: $(EXE)
	$(EXE)

include $(ML_ROOT)/make/common.mk
