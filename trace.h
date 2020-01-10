#ifndef __TRACE_H__
#define __TRACE_H__

#include <stdio.h>
#include <stdint.h>

typedef	uint8_t			u8;
typedef	uint16_t		u16;
typedef	uint32_t		u32;
typedef	uint64_t		u64;
typedef	int8_t			s8;
typedef	int16_t			s16;
typedef	int32_t			s32;
typedef	int64_t			s64;

#define	TRACE_PROT_NONE		0x00
#define	TRACE_PROT_READ		0x01
#define	TRACE_PROT_WRITE	0x02
#define	TRACE_PROT_EXEC		0x04

#define	TRACE_MAP_SHARED	0x01
#define TRACE_MAP_PRIVATE	0x02
#define TRACE_MAP_FIXED		0x10
#define TRACE_MAP_ANONYMOUS	0x20

#define	TRACE_LITTLE_ENDIAN	0x00
#define	TRACE_BIG_ENDIAN	0x01
#define	TRACE_NO_VALUE		0x00
#define	TRACE_HAS_VALUE		0x02

#define	TRACE_TYPE_OTHER	0x00
#define	TRACE_TYPE_JCC		0x01
#define	TRACE_TYPE_JMP		0x02
#define	TRACE_TYPE_JMP_INDIRECT	0x03
#define	TRACE_TYPE_CALL		0x04
#define	TRACE_TYPE_RET		0x05
#define	TRACE_TYPE_RTI		0x07

#define DEFINE(y, z, x) \
	typedef struct { x } y; \
	const char* z = #x;

typedef struct stringnode STRINGNODE;

struct stringnode {
	STRINGNODE*	next;
	STRINGNODE*	children;
	unsigned int	id;
	char		c;
};

typedef struct {
	FILE*		file;
	STRINGNODE*	strings;
	unsigned int	stringcnt;
	int		endianess;
	int		stepsize;
} TRACE;

TRACE*	TRACEOpen(const char* filename, const char* format, const char* structdef, size_t structsize, size_t pcoff, size_t pcsize, size_t stepoff, size_t stepsize, int endianess);
void	TRACEClose(TRACE* trace);
void	TRACEStep(TRACE* trace, u32 tid, const void* step, const char* machinecode, const int machinecode_size, const char** assembly, const char type);
void	TRACEMap(TRACE* trace, u32 tid, u64 addr, u64 len, int prot, int flags, u64 offset, u32 fd, u64 result, const char* filename);
void	TRACEUnmap(TRACE* trace, u32 tid, u64 addr, u64 len, u64 result);
void	TRACEWriteI8(TRACE* trace, u32 tid, u64 addr, u8 value);
void	TRACEWriteI16(TRACE* trace, u32 tid, u64 addr, u16 value);
void	TRACEWriteI32(TRACE* trace, u32 tid, u64 addr, u32 value);
void	TRACEWriteI64(TRACE* trace, u32 tid, u64 addr, u64 value);
void	TRACEReadI8(TRACE* trace, u32 tid, u64 addr, u8 value);
void	TRACEReadI16(TRACE* trace, u32 tid, u64 addr, u16 value);
void	TRACEReadI32(TRACE* trace, u32 tid, u64 addr, u32 value);
void	TRACEReadI64(TRACE* trace, u32 tid, u64 addr, u64 value);
void	TRACEReadI8Fault(TRACE* trace, u32 tid, u64 addr);
void	TRACEReadI16Fault(TRACE* trace, u32 tid, u64 addr);
void	TRACEReadI32Fault(TRACE* trace, u32 tid, u64 addr);
void	TRACEReadI64Fault(TRACE* trace, u32 tid, u64 addr);

#endif
