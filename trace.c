#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "trace.h"

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define	U16B(x)		(x)
#define	U32B(x)		(x)
#define	U64B(x)		(x)
#define	U16L(x)		__builtin_bswap16(x)
#define	U32L(x)		__builtin_bswap32(x)
#define	U64L(x)		__builtin_bswap64(x)
#else
#define	U16B(x)		__builtin_bswap16(x)
#define	U32B(x)		__builtin_bswap32(x)
#define	U64B(x)		__builtin_bswap64(x)
#define	U16L(x)		(x)
#define	U32L(x)		(x)
#define	U64L(x)		(x)
#endif

static const char MAGIC[6] = { 'X', 'T', 'R', 'C', 0xFF, 0xFF };

typedef struct {
	u32	magic;
	u32	tid;
	u64	addr;
	u64	len;
	u64	off;
	u64	result;
	u32	prot;
	u32	flags;
	u32	fd;
	u16	filename;
} MMAPEVT;

typedef struct {
	u32	magic;
	u32	tid;
	u64	addr;
	u64	len;
	u32	result;
} MUNMAPEVT;

typedef struct {
	u32	magic;
	u32	tid;
	u64	addr;
	u64	value;
	u8	size;
	u8	flags;
} MEMEVT;

typedef struct {
	u16	size;
	u16	pcoff;
	u16	pcsize;
	u16	stepoff;
	u16	stepsize;
} TRACEHEADER;

typedef struct {
	u32	magic;
	u32	tid;
} RECORDHEADER;

unsigned int TRACEGetString(TRACE* trace, const char* string)
{
	const char* s = string;
	STRINGNODE* node = trace->strings;

	while(*s) {
		if(node->c == *s) { /* match: advance in string */
			if(!node->children) {
				node->children = (STRINGNODE*) malloc(sizeof(STRINGNODE));
				node->children->next = NULL;
				node->children->children = NULL;
				node->children->c = *s;
				node->children->id = -1;
			}
			node = node->children;
			s++;
		} else if(node->next) { /* no match: continue on same level */
			node = node->next;
		} else { /* no match and no next node: create new node and advance in string */
			node->next = (STRINGNODE*) malloc(sizeof(STRINGNODE));
			node->next->next = NULL;
			node->next->children = NULL;
			node->next->c = *s;
			node->next->id = -1;
		}
	}

	/* find leaf node */
	if(node->children) {
		STRINGNODE* new;

		/* search leaf node */
		node = node->children;
		while(node->next) {
			if(node->c == 0) { /* found: return */
				return node->id;
			} else { /* not found: continue */
				node = node->next;
			}
		}

		/* last node? */
		if(node->c == 0) {
			return node->id;
		}

		/* not found: create */
		new = (STRINGNODE*) malloc(sizeof(STRINGNODE));
		new->next = NULL;
		new->children = NULL;
		new->id = ++trace->stringcnt;
		new->c = 0;
		node->next = new;
		return -1;
	} else {
		/* no leaf node: create */
		STRINGNODE* new = (STRINGNODE*) malloc(sizeof(STRINGNODE));
		new->next = NULL;
		new->children = NULL;
		new->id = ++trace->stringcnt;
		new->c = 0;
		node->children = new;
		return -1;
	}
}

TRACE* TRACEOpen(const char* filename, const char* format, const char* structdef, size_t structsize, size_t pcoff, size_t pcsize, size_t stepoff, size_t stepsize, int endianess)
{
	TRACEHEADER hdr;
	u8 endian;
	u16 len;
	int tmp;

	TRACE* trace = (TRACE*) malloc(sizeof(TRACE));
	if(!trace)
		return NULL;

	/* open file and write magic */
	trace->file = fopen(filename, "wb");
	if(!trace->file) {
		free(trace);
		return NULL;
	}

	if(fwrite(MAGIC, sizeof(MAGIC), 1, trace->file) != 1)
		goto err;

	hdr.size = U16B(structsize);
	hdr.pcoff = U16B(pcoff);
	hdr.pcsize = U16B(pcsize);
	hdr.stepoff = U16B(stepoff);
	hdr.stepsize = U16B(stepsize);

	if(fwrite(&hdr, sizeof(hdr), 1, trace->file) != 1)
		goto err;

	len = U16B(strlen(structdef));
	if(fwrite(&len, 2, 1, trace->file) != 1)
		goto err;
	if(fwrite(structdef, strlen(structdef), 1, trace->file) != 1)
		goto err;

	len = U16B(strlen(format));
	if(fwrite(&len, 2, 1, trace->file) != 1)
		goto err;
	if(fwrite(format, strlen(format), 1, trace->file) != 1)
		goto err;

	endian = (u8) endianess;
	if(fwrite(&endian, 1, 1, trace->file) != 1)
		goto err;

	trace->endianess = endianess;
	trace->stepsize = structsize;

	/* create strings trie */
	trace->strings = (STRINGNODE*) malloc(sizeof(STRINGNODE));
	trace->strings->next = NULL;
	trace->strings->children = NULL;
	trace->strings->c = 0;
	trace->strings->id = 0;

	return trace;

err:
	tmp = errno;
	fclose(trace->file);
	free(trace);
	errno = tmp;
	return NULL;
}

void TRACEClose(TRACE* trace)
{
	fclose(trace->file);
	free(trace);
}

void TRACEStep(TRACE* trace, u32 tid, const void* step, const char* machinecode, const int machinecode_size, const char** assembly, const char type)
{
	u8 cnt;
	u16 len;
	const char** s;
	RECORDHEADER hdr;
	memcpy(&hdr.magic, "STEP", 4);
	hdr.tid = U32B(tid);
	fwrite(&hdr, sizeof(hdr), 1, trace->file);
	fwrite(step, trace->stepsize, 1, trace->file);
	for(s = assembly, cnt = 0; *s; s++, cnt++);
	fwrite(&cnt, 1, 1, trace->file);
	for(s = assembly; *s; s++) {
		unsigned int id = TRACEGetString(trace, *s);
		u32 id32 = U32B(id);
		fwrite(&id32, 4, 1, trace->file);
		if(id == -1) {
			len = U16B(strlen(*s));
			fwrite(&len, 2, 1, trace->file);
			fwrite(*s, strlen(*s), 1, trace->file);
		}
	}
	len = U16B(machinecode_size);
	fwrite(&len, 2, 1, trace->file);
	fwrite(machinecode, machinecode_size, 1, trace->file);
	fwrite(&type, 1, 1, trace->file);
}

void TRACEMap(TRACE* trace, u32 tid, u64 addr, u64 len, int prot, int flags, u64 offset, u32 fd, u64 result, const char* filename)
{
	MMAPEVT evt = { 0 };
	memcpy(&evt.magic, "MMAP", 4);
	evt.addr = U64B(addr);
	evt.len = U64B(len);
	evt.prot = U32B(prot);
	evt.flags = U32B(flags);
	evt.off = U64B(offset);
	evt.fd = U32B(fd);
	evt.result = U64B(result);

	if(filename) {
		evt.filename = U16B(strlen(filename));
		fwrite(&evt, 54, 1, trace->file);
		fwrite(filename, strlen(filename), 1, trace->file);
	} else {
		fwrite(&evt, 54, 1, trace->file);
	}
}

void TRACEUnmap(TRACE* trace, u32 tid, u64 addr, u64 len, u64 result)
{
	MUNMAPEVT evt = { 0 };
	memcpy(&evt.magic, "UMAP", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.len = U64B(len);
	evt.result = U32B(len);
	fwrite(&evt, 28, 1, trace->file);
}

void TRACEWriteI8(TRACE* trace, u32 tid, u64 addr, u8 value)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMW", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = U64B(value);
	evt.size = 1;
	evt.flags = TRACE_HAS_VALUE | trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}

void TRACEWriteI16(TRACE* trace, u32 tid, u64 addr, u16 value)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMW", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = U64B(value);
	evt.size = 2;
	evt.flags = TRACE_HAS_VALUE | trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}

void TRACEWriteI32(TRACE* trace, u32 tid, u64 addr, u32 value)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMW", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = U64B(value);
	evt.size = 4;
	evt.flags = TRACE_HAS_VALUE | trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}

void TRACEWriteI64(TRACE* trace, u32 tid, u64 addr, u64 value)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMW", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = U64B(value);
	evt.size = 8;
	evt.flags = TRACE_HAS_VALUE | trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}

void TRACEReadI8(TRACE* trace, u32 tid, u64 addr, u8 value)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMR", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = U64B(value);
	evt.size = 1;
	evt.flags = TRACE_HAS_VALUE | trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}

void TRACEReadI16(TRACE* trace, u32 tid, u64 addr, u16 value)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMR", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = U64B(value);
	evt.size = 2;
	evt.flags = TRACE_HAS_VALUE | trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}

void TRACEReadI32(TRACE* trace, u32 tid, u64 addr, u32 value)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMR", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = U64B(value);
	evt.size = 4;
	evt.flags = TRACE_HAS_VALUE | trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}

void TRACEReadI64(TRACE* trace, u32 tid, u64 addr, u64 value)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMR", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = U64B(value);
	evt.size = 8;
	evt.flags = TRACE_HAS_VALUE | trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}

void TRACEReadI8Fault(TRACE* trace, u32 tid, u64 addr)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMR", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = 0;
	evt.size = 1;
	evt.flags = trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}

void TRACEReadI16Fault(TRACE* trace, u32 tid, u64 addr)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMR", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = 0;
	evt.size = 2;
	evt.flags = trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}

void TRACEReadI32Fault(TRACE* trace, u32 tid, u64 addr)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMR", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = 0;
	evt.size = 4;
	evt.flags = trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}

void TRACEReadI64Fault(TRACE* trace, u32 tid, u64 addr)
{
	MEMEVT evt = { 0 };
	memcpy(&evt.magic, "MEMR", 4);
	evt.tid = U32B(tid);
	evt.addr = U64B(addr);
	evt.value = 0;
	evt.size = 8;
	evt.flags = trace->endianess;
	fwrite(&evt, 26, 1, trace->file);
}
