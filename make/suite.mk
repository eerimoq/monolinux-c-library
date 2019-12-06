BUILD = $(shell readlink -f build)
EXE = $(BUILD)/suite
CFLAGS += -fno-omit-frame-pointer
# CFLAGS += -fsanitize=address
CFLAGS += -coverage
CFLAGS += -Wall -Wextra -std=gnu11
CFLAGS += -g -Og
CFLAGS += -DUNIT_TEST
CFLAGS += -no-pie
LDFLAGS_MOCKS = $(shell cat $(BUILD)/nala_mocks.ld)
COVERAGE_FILTERS +=
INC += $(ML_ROOT)/tst
INC += $(ML_ROOT)/tst/utils
INC += $(BUILD)
SRC += $(ML_ROOT)/tst/utils/nala.c
SRC += $(ML_ROOT)/tst/utils/mock.c
SRC += $(ML_ROOT)/tst/utils/utils.c
SRC += $(BUILD)/nala_mocks.c
SRC += $(TESTS)
NALA = nala
MAIN_C =
TESTS ?= main.c
TESTS_O = $(patsubst %,$(BUILD)%,$(abspath $(TESTS:%.c=%.o)))

.PHONY: all run build coverage

all: run

build:
	$(MAKE) $(EXE)

run: build
	$(EXE)

test: run
	$(MAKE) coverage

$(TESTS_O): $(BUILD)/nala_mocks.c

$(BUILD)/nala_mocks.c: $(TESTS)
	echo "MOCK $^"
	mkdir -p $(BUILD)
	[ -f nala_mocks.h ] || touch $(BUILD)/nala_mocks.h
	cat $(TESTS) > tests.pp.c
	$(CC) $(INC:%=-I%) -D_GNU_SOURCE=1 -E tests.pp.c \
	    | $(NALA) generate_mocks -o $(BUILD)

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
