# A package.

CLEAN_PATHS ?=

.PHONY: test clean

test:
	$(MAKE) -C tst

clean:
	$(MAKE) -C tst clean
	rm -rf build $(CLEAN_PATHS)

print-%:
	@echo $($*)
