SUITE ?= all

ifneq ($(SUITE), all)
TESTS = $(SUITE:%=test_%.c)
endif

BUILD = build/$(SUITE)
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

.PHONY: all run build coverage

all: run

build: $(BUILD)/nala_mocks.h
	$(MAKE) $(EXE)

run: build
	$(EXE)

test: run
	$(MAKE) coverage

$(BUILD)/nala_mocks.h: $(TESTS)
	echo "MOCKGEN $^"
	mkdir -p $(BUILD)
	[ -f nala_mocks.h ] || touch $(BUILD)/nala_mocks.h
	cat $(TESTS) > tests.pp.c
	$(CC) $(INC:%=-I%) -D_GNU_SOURCE=1 -E tests.pp.c \
	    | $(NALA) generate_mocks -o $(BUILD)

coverage:
	gcovr --root ../.. \
	    --exclude-directories ".*tst.*" $(COVERAGE_FILTERS:%=-f %) \
	    --html-details --output index.html $(BUILD)
	mkdir -p $(BUILD)/coverage
	mv index.* $(BUILD)/coverage
	@echo
	@echo "Code coverage report: $$(readlink -f $(BUILD)/coverage/index.html)"
	@echo

include $(ML_ROOT)/make/common.mk
