LDFLAGS += -Wl,--wrap=bind
LDFLAGS += -Wl,--wrap=setsockopt
LDFLAGS += -Wl,--wrap=sendto
LDFLAGS += -Wl,--wrap=ml_read
LDFLAGS += -Wl,--wrap=timerfd_settime
LDFLAGS += -Wl,--wrap=poll
LDFLAGS += -Wl,--wrap=ml_network_interface_mac_address
LDFLAGS += -Wl,--wrap=ml_timer_handler_init
INC += ..
SRC += ../utils/utils.c
SRC += ../utils/mocks/mock.c
SRC += ../utils/mocks/mock_libc.c

include $(ML_ROOT)/make/suite.mk
