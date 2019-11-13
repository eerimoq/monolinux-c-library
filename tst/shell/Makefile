INC += ..
SRC += ../utils/utils.c
SRC += ../utils/mocks/mock.c
SRC += ../utils/mocks/mock_libc.c
LDFLAGS += -Wl,--wrap=statvfs
LDFLAGS += -Wl,--wrap=setmntent
LDFLAGS += -Wl,--wrap=getmntent
LDFLAGS += -Wl,--wrap=endmntent
LDFLAGS += -Wl,--wrap=nftw
COVERAGE_FILTERS = .*ml_shell.c

include $(ML_ROOT)/make/suite.mk
