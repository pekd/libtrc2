#include <stddef.h>
#include "trace.h"

DEFINE(VMSTEP, vmstep_def,
		u64	step;
		u16	pc;
		u8	a;
		u8	x;
		u8	y;
		u8	flags;
);

const char* format =
		"A=${a;x2} X=${x;x2} Y=${y;x2} FLAGS=${flags;x2}\n"
		"PC=${pc;x4}\n";

int main(void)
{
	VMSTEP step = { 0 };
	const char* asm1[] = { "mov", "a", "42", NULL };
	const char* asm2[] = { "mov", "x", "43", NULL };
	const char* asm3[] = { "mov", "y", "44", NULL };
	const char* asm4[] = { "nop", NULL };
	u8 machinecode[1];

	/* open trace file: use our VMSTEP struct, format string, and define it
	 * as little endian
	 *
	 * Note: endianess affects the interpretation of our VMSTEP *and* the
	 * layout of the VM's memory (= read/write operations)!
	 * It is important to either disable alignment or properly align the
	 * fields in the VMSTEP struct such that no padding is introduced
	 * between fields. Otherwise trcview will not decode the struct
	 * correctly.
	 */
	TRACE* trace = TRACEOpen("demo.trc", format, vmstep_def, sizeof(VMSTEP),
			offsetof(VMSTEP, pc), sizeof(step.pc),
			offsetof(VMSTEP, step), sizeof(step.step),
			TRACE_LITTLE_ENDIAN);

	/* map two memory pages */
	TRACEMap(trace, 0, 0, 4096, TRACE_PROT_READ, TRACE_MAP_PRIVATE|TRACE_MAP_ANONYMOUS|TRACE_MAP_FIXED, 0, 0, 0, NULL);
	TRACEMap(trace, 0, 4096, 4096, TRACE_PROT_READ|TRACE_PROT_WRITE, TRACE_MAP_PRIVATE|TRACE_MAP_ANONYMOUS, 0, 0, 4096, "data");

	/* write steps */
	machinecode[0] = 42;
	step.pc = 0;
	step.step = 0;
	TRACEStep(trace, 0, &step, (const char*) machinecode, 1, asm1, TRACE_TYPE_OTHER);
	step.a = 42;

	machinecode[0] = 43;
	step.pc = 1;
	step.step = 1;
	TRACEStep(trace, 0, &step, (const char*) machinecode, 1, asm2, TRACE_TYPE_OTHER);
	step.x = 43;

	/* write "Noodle" to memory starting at address 42
	 * Note: this write belongs to step 1 */
	TRACEWriteI32(trace, 0, 42, 0x646F6F4E);
	TRACEWriteI16(trace, 0, 46, 0x656C);

	machinecode[0] = 44;
	step.pc = 2;
	step.step = 2;
	TRACEStep(trace, 0, &step, (const char*) machinecode, 1, asm3, TRACE_TYPE_OTHER);
	step.y = 44;

	/* read one of the bytes that were written earlier
	 * Note: thins read belongs to step 2 */
	TRACEReadI8(trace, 0, 44, 0x6F);

	machinecode[0] = 0;
	step.pc = 3;
	step.step = 3;
	TRACEStep(trace, 0, &step, (const char*) machinecode, 1, asm4, TRACE_TYPE_OTHER);

	/* unmap both pages */
	TRACEUnmap(trace, 0, 0, 8192, 0);

	/* close trace file */
	TRACEClose(trace);
}
