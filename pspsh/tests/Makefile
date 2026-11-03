CXX ?= c++

.PHONY: test test-parse-args test-disasm
test: test-parse-args test-disasm

test-parse-args:
	@bin=$$(mktemp /tmp/psplinkusb-pspsh-quotes.XXXXXX); \
	trap 'rm -f "$$bin"' EXIT; \
	$(CXX) -std=c++11 -Wall -Werror -Wextra -O1 -g -I.. -o "$$bin" parse_args_unmatched_quotes_test.cpp ../parse_args.cpp && "$$bin"

test-disasm:
	@bin=$$(mktemp /tmp/psplinkusb-pspsh-disasm.XXXXXX); \
	trap 'rm -f "$$bin"' EXIT; \
	$(CXX) -std=c++11 -Wall -Werror -Wextra -O1 -g -I.. -o "$$bin" disasm_output_test.cpp ../disasm.cpp && "$$bin"
