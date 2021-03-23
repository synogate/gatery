#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
	uint32_t valid;
	uint32_t key[1];
} TinyCuckooItem;

typedef struct {
	uint32_t capacity;
	uint16_t numTables;
	uint16_t keyWords;
	uint16_t valueWords;
	uint16_t itemWords;
	uint16_t hashBitPerTable;
	uint16_t hashWords;

	uint32_t limitChainJobs;
	uint32_t limitChainDepth;

	void* hashCtx;
	void (*hash)(void* ctx, uint32_t* key, uint32_t* hash_out);

	void* (*alloc)(size_t);
	void (*free)(void*, size_t);

	void* mmCtx;
	void (*mmwrite)(void* ctx, uint32_t offset, uint32_t value);

	uint32_t items[1];
} TinyCuckooContext;

TinyCuckooContext* tiny_cuckoo_init(
	uint32_t capacity, uint32_t numTables, 
	uint32_t keyWidth, size_t valueWidth,
	void* (*allocator_proc)(size_t), void (*free_proc)(void*, size_t)
);
void tiny_cuckoo_destroy(TinyCuckooContext* ctx);

void tiny_cuckoo_set_hash(TinyCuckooContext* ctx, void (*hash_proc)(void*, uint32_t*, uint32_t*), void* userData);
void tiny_cuckoo_set_limits(TinyCuckooContext* ctx, uint32_t numChainJobs, uint32_t maxChainDepth);
void tiny_cuckoo_set_mm(TinyCuckooContext* ctx, void (*mmwrite)(void* ctx, uint32_t offset, uint32_t value), void* userData);
uint32_t tiny_cuckoo_get_hashwidth(TinyCuckooContext* ctx);

int tiny_cuckoo_update(TinyCuckooContext* ctx, uint32_t* key, uint32_t* value);
uint32_t* tiny_cuckoo_lookup(TinyCuckooContext* ctx, uint32_t* key);
int tiny_cuckoo_remove(TinyCuckooContext* ctx, uint32_t* key);
