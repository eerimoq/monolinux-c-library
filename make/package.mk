# A package.

SUITES ?= $(filter-out utils,$(shell ls tst))
CLEAN_PATHS ?=

.PHONY: test clean coverage

test: run
	$(MAKE) coverage

run:
	for test in $(SUITES) ; do \
	  echo ; \
	  echo "============================ $$test ============================" ; \
	  $(MAKE) -C tst/$$test run || exit 1 ; \
	done

coverage:
	gcovr --exclude tst --html-details --output index.html
	mkdir -p build/coverage
	mv index.* build/coverage
	@echo
	@echo "Code coverage report: $$(readlink -f build/coverage/index.html)"
	@echo

clean:
	for test in $(SUITES) ; do \
	  $(MAKE) -C tst/$$test clean ; \
	done
	rm -rf build $(CLEAN_PATHS)

print-%:
	@echo $($*)
