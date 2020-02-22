BUILD = build
EXE = $(BUILD)/suite
CFLAGS += -fno-omit-frame-pointer
ifeq ($(SANITIZE), yes)
CFLAGS += -fsanitize=address
CFLAGS += -fsanitize=undefined
endif
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
TESTS ?= main.c

.PHONY: all run build coverage clean

all: run

build:
	$(MAKE) $(BUILD)/nala_mocks.ld
	$(MAKE) $(EXE)

run: build
	$(EXE) $(ARGS)

test: run
	$(MAKE) coverage

$(BUILD)/nala_mocks.ld: $(TESTS)
	echo "MOCKGEN $^"
	mkdir -p $(BUILD)
	[ -f $(BUILD)/nala_mocks.h ] || touch $(BUILD)/nala_mocks.h
	cat $(TESTS) > tests.pp.c
	$(CC) $(INC:%=-I%) -D_GNU_SOURCE=1 -DNALA_GENERATE_MOCKS -E tests.pp.c \
	    | $(NALA) generate_mocks -o $(BUILD)
	touch $@

coverage:
	gcovr --root ../.. \
	    --exclude-directories ".*tst.*" $(COVERAGE_FILTERS:%=-f %) \
	    --html-details --output index.html $(BUILD)
	mkdir -p $(BUILD)/coverage
	mv index.* $(BUILD)/coverage
	@echo
	@echo "Code coverage report: $$(readlink -f $(BUILD)/coverage/index.html)"
	@echo

clean:
	rm -rf $(BUILD) $(CLEAN)

include $(ML_ROOT)/make/common.mk
