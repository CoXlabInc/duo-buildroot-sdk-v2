SHELL = /bin/bash
#
ifneq ($(BUILD_PATH),)
include $(BUILD_PATH)/.config
endif
#
chip_arch = $(shell echo $(CHIP_ARCH) | tr A-Z a-z)

all:
	+$(MAKE) -C common all
	+$(MAKE) -C sensor/$(chip_arch) all

clean:
	+$(MAKE) -C common clean
	+$(MAKE) -C sensor/$(chip_arch) clean
