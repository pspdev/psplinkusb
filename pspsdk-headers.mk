# Native clients only need the canonical controller header, not a PSP compiler.
# For host-only builds, point PSPSDK_CTRL_DIR at a PSPSDK checkout's src/ctrl.
PSPSDK_CTRL_DIR ?= $(shell psp-config --pspsdk-path 2>/dev/null)/include
PSPCTRL_HEADER := $(PSPSDK_CTRL_DIR)/pspctrl.h

# Search only quoted includes: an installed SDK also contains PSP networking
# and sys headers which must not shadow the native platform headers.
PSPCTRL_CPPFLAGS := -iquote "$(PSPSDK_CTRL_DIR)"

ifeq ($(wildcard $(PSPCTRL_HEADER)),)
$(PSPCTRL_HEADER):
	@echo "Error: PSPSDK pspctrl.h not found. Set PSPSDK_CTRL_DIR to PSPSDK's src/ctrl or installed include directory." >&2
	@exit 1
endif
