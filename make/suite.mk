BUILD = build
EXE = $(BUILD)/suite
CFLAGS += -fno-omit-frame-pointer
ifeq ($(SANITIZE), yes)
CFLAGS += -fsanitize=address
CFLAGS += -fsanitize=undefined
endif
CFLAGS += -coverage
CFLAGS += -Wall -Wextra -std=gnu11 -Werror
CFLAGS += -g -O0
CFLAGS += -DUNIT_TEST
CFLAGS += -no-pie
CFLAGS += -DNALA_INCLUDE_NALA_MOCKS_H
CFLAGS += -DML_LOG_OBJECT_TXT='"log_object.txt"'
LDFLAGS_MOCKS = $(shell cat $(BUILD)/nala_mocks.ldflags)
COVERAGE_FILTERS +=
INC += $(ML_ROOT)/tst
INC += $(ML_ROOT)/tst/utils
INC += $(shell $(NALA) include_dir)
INC += $(BUILD)
SRC += $(shell $(NALA) c_sources)
SRC += $(ML_ROOT)/tst/utils/utils.c
SRC += $(BUILD)/nala_mocks.c
SRC += $(TESTS)
NALA = nala
WRAP_INTERNAL_SYMBOLS = $(NALA) wrap_internal_symbols $(BUILD)/nala_mocks.ldflags
TESTS ?= main.c
LSAN_OPTIONS = \
	suppressions=$(ML_ROOT)/make/lsan-suppressions.txt \
	print_suppressions=0
IMPLEMENTATION += fprintf
NO_IMPLEMENTATION += ioctl

.PHONY: all run build coverage clean

all: run

build:
	$(MAKE) $(BUILD)/nala_mocks.ldflags
	$(MAKE) $(EXE)

run: build
	LSAN_OPTIONS="$(LSAN_OPTIONS)" $(EXE) $(ARGS)

test: run
	$(MAKE) coverage

$(BUILD)/nala_mocks.ldflags: $(TESTS)
	echo "MOCKGEN $^"
	mkdir -p $(BUILD)
	[ -f $(BUILD)/nala_mocks.h ] || touch $(BUILD)/nala_mocks.h
	$(NALA) cat $(TESTS) > tests.pp.c
	$(CC) $(INC:%=-I%) -D_GNU_SOURCE=1 -DNALA_GENERATE_MOCKS -E tests.pp.c \
	    | $(NALA) generate_mocks \
	          $(IMPLEMENTATION:%=-i %) $(NO_IMPLEMENTATION:%=-n %) -o $(BUILD)
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

# Recursive make for helpful output.
gdb:
	test_file_func=$$($(EXE) --print-test-file-func $(TEST)) && \
	$(MAKE) gdb-run TEST_FILE_FUNC=$$test_file_func

gdb-run:
	gdb \
	    -ex "b $(TEST_FILE_FUNC)_before_fork" \
	    -ex "r $(TEST)" \
	    -ex "set follow-fork-mode child" \
	    -ex c \
	    $(EXE)

clean:
	rm -rf $(BUILD) $(CLEAN)

include $(ML_ROOT)/make/common.mk
