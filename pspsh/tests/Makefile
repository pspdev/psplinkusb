WERROR ?= $(if $(filter true 1,$(CI)),1,0)

CXX ?= c++

.PHONY: test test-parse-args test-disasm test-joy test-joy-cli
test: test-parse-args test-disasm test-joy test-joy-cli

include ../../pspsdk-headers.mk

test-parse-args:
	@bin=$$(mktemp /tmp/psplinkusb-pspsh-quotes.XXXXXX); \
	trap 'rm -f "$$bin"' EXIT; \
	$(CXX) -std=c++11 -Wall $(if $(filter 1,$(WERROR)),-Werror) -Wextra -O1 -g -I.. -o "$$bin" parse_args_unmatched_quotes_test.cpp ../parse_args.cpp && "$$bin"

test-disasm:
	@bin=$$(mktemp /tmp/psplinkusb-pspsh-disasm.XXXXXX); \
	trap 'rm -f "$$bin"' EXIT; \
	$(CXX) -std=c++11 -Wall $(if $(filter 1,$(WERROR)),-Werror) -Wextra -O1 -g -I.. -o "$$bin" disasm_output_test.cpp ../disasm.cpp && "$$bin"

test-joy: $(PSPCTRL_HEADER)
	@bin=$$(mktemp /tmp/psplinkusb-pspsh-joy.XXXXXX); \
	trap 'rm -f "$$bin"' EXIT; \
	$(CXX) -std=c++11 -Wall $(if $(filter 1,$(WERROR)),-Werror) -Wextra -O1 -g -I.. $(PSPCTRL_CPPFLAGS) -o "$$bin" joy_command_test.cpp ../joy.cpp && "$$bin"

test-joy-cli: $(PSPCTRL_HEADER)
	@$(MAKE) -C .. all WERROR=$(WERROR)
	@bin=$$(mktemp /tmp/psplinkusb-pspsh-joy-cli.XXXXXX); \
	trap 'rm -f "$$bin"' EXIT; \
	$(CXX) -std=c++11 -Wall $(if $(filter 1,$(WERROR)),-Werror) -Wextra -O1 -g -I.. $(PSPCTRL_CPPFLAGS) -o "$$bin" joy_cli_test.cpp && "$$bin" ../pspsh
