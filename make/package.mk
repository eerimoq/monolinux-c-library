# A package.

BUILD ?= build
CLEAN_PATHS ?=
LIBRARY = $(BUILD)/libml.a
PREFIX ?= /usr/local

.PHONY: test clean library install

run:
	$(MAKE) -C tst run

test:
	$(MAKE) -C tst test

clean:
	$(MAKE) -C tst clean
	rm -rf $(BUILD) $(CLEAN_PATHS)

library: $(LIBRARY)

include $(ML_ROOT)/make/common.mk

$(LIBRARY): $(OBJ)
	mkdir -p $(BUILD)
	$(AR) cr $(LIBRARY) $^

install:
	mkdir -p $(PREFIX)/include/ml
	install -m 644 include/ml/ml.h $(PREFIX)/include/ml
	mkdir -p $(PREFIX)/lib
	install -m 644 $(LIBRARY) $(PREFIX)/lib
