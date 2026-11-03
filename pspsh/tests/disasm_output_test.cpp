#include <stdio.h>
#include <string.h>

#include "../disasm.h"

struct DisasmCase
{
	unsigned int opcode;
	const char *expected;
};

int main(void)
{
	/* Cover the pair, triple, and quad VFPU rotators, including alternate
	 * swizzles. Each operand must append rather than replace earlier text. */
	static const DisasmCase cases[] = {
		{ 0xF3A00080U, "vrot.p     C000, S000, [c, s]" },
		{ 0xF3A08000U, "vrot.t     C000, S000, [c, s, s]" },
		{ 0xF3A08080U, "vrot.q     C000, S000, [c, s, s, s]" },
		{ 0xF3A30080U, "vrot.p     C000, S000, [s, 0]" },
		{ 0xF3A18080U, "vrot.q     C000, S000, [s, c, 0, 0]" },
	};
	unsigned int i;

	for(i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		const char *actual = disasmInstruction(cases[i].opcode, 0x08800000U,
			NULL, NULL, 1);
		if(strcmp(actual, cases[i].expected) != 0)
		{
			fprintf(stderr, "Disassembly mismatch for 0x%08X:\n  expected: %s\n  actual:   %s\n",
				cases[i].opcode, cases[i].expected, actual);
			return 1;
		}
	}

	puts("pspsh disassembler output test passed");
	return 0;
}
