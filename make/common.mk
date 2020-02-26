INC += $(ML_ROOT)/include
MAIN_C ?=
SRC += $(MAIN_C)
SRC += $(ML_ROOT)/src/ml.c
SRC += $(ML_ROOT)/src/ml_bus.c
SRC += $(ML_ROOT)/src/ml_device_mapper.c
SRC += $(ML_ROOT)/src/ml_dhcp_client.c
SRC += $(ML_ROOT)/src/ml_ntp_client.c
SRC += $(ML_ROOT)/src/ml_inet.c
SRC += $(ML_ROOT)/src/ml_libc.c
SRC += $(ML_ROOT)/src/ml_log_object.c
SRC += $(ML_ROOT)/src/ml_message.c
SRC += $(ML_ROOT)/src/ml_network.c
SRC += $(ML_ROOT)/src/ml_queue.c
SRC += $(ML_ROOT)/src/ml_rtc.c
SRC += $(ML_ROOT)/src/ml_shell.c
SRC += $(ML_ROOT)/src/ml_timer.c
SRC += $(ML_ROOT)/src/ml_worker_pool.c
OBJ = $(patsubst %,$(BUILD)%,$(abspath $(SRC:%.c=%.o)))
CFLAGS += $(INC:%=-I%)
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -D_GNU_SOURCE=1
LDFLAGS += -Wl,--gc-sections -L$(BUILD)/root/lib $(LIBS:%=-l%)
STRIP ?= no
DEPSDIR = $(BUILD)/deps

.PHONY: app

app: $(EXE)

$(EXE): $(OBJ)
	@echo "LD $@"
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDFLAGS_MOCKS) -lpthread -o $@
ifeq ($(STRIP), yes)
	$(CROSS_COMPILE)strip $@
endif

define COMPILE_template
-include $(patsubst %.c,$(DEPSDIR)%.o.dep,$(abspath $1))
$(patsubst %.c,$(BUILD)%.o,$(abspath $1)): $1
	@echo "CC $1"
	mkdir -p $(BUILD)$(abspath $(dir $1))
	mkdir -p $(DEPSDIR)$(abspath $(dir $1))
	$$(CC) $$(CFLAGS) -c -o $$@ $$<
	gcc -MM -MT $$@ $$(CFLAGS) -o $(patsubst %.c,$(DEPSDIR)%.o.dep,$(abspath $1)) $$<
endef
$(foreach file,$(SRC),$(eval $(call COMPILE_template,$(file))))
