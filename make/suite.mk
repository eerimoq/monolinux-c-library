BUILD = $(shell readlink -f build)
EXE = $(BUILD)/suite
CFLAGS += -fno-omit-frame-pointer
# CFLAGS += -fsanitize=address
CFLAGS += -coverage
CFLAGS += -Wall -Wextra -std=gnu11
CFLAGS += -g -Og
CFLAGS += -DUNIT_TEST
COVERAGE_FILTERS +=
INC += $(ML_ROOT)/tst/utils
SRC += $(ML_ROOT)/tst/utils/nala.c
SRC += nala_mocks.c
NALA = nala

.PHONY: all run build coverage

all: run
	$(MAKE) coverage

build: nala_mocks.h
	$(MAKE) $(EXE)

run: build
	$(EXE)

nala_mocks.h: main.c
	[ -f nala_mocks.h ] || touch nala_mocks.h
	$(CC) $(INC:%=-I%) -E main.c | $(NALA) generate_mocks

coverage:
	gcovr --root ../.. \
	    --exclude-directories ".*tst.*" $(COVERAGE_FILTERS:%=-f %) \
	    --html-details --output index.html build
	mkdir -p $(BUILD)/coverage
	mv index.* $(BUILD)/coverage
	@echo
	@echo "Code coverage report: $$(readlink -f build/coverage/index.html)"
	@echo

include $(ML_ROOT)/make/common.mk
