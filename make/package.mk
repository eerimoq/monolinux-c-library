# A package.

CLEAN_PATHS ?=

.PHONY: test clean

run:
	$(MAKE) -C tst run

test:
	$(MAKE) -C tst test

clean:
	$(MAKE) -C tst clean
	rm -rf build $(CLEAN_PATHS)

print-%:
	@echo $($*)
