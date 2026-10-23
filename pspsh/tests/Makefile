CXX ?= c++

.PHONY: test
test:
	@bin=$$(mktemp /tmp/psplinkusb-pspsh-quotes.XXXXXX); \
	trap 'rm -f "$$bin"' EXIT; \
	$(CXX) -std=c++11 -Wall -Wextra -O1 -g -I.. -o "$$bin" parse_args_unmatched_quotes_test.cpp ../parse_args.cpp && "$$bin"
