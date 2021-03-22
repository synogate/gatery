#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {

	uint32_t keyWidth;
	uint32_t hashWidth;

	void* (*alloc)(size_t);
	void (*free)(void*, size_t);

	void* mmCtx;
	void (*mmwrite)(void* ctx, uint32_t offset, uint32_t value);

	uint32_t items[1];

} TabulationHashingContext;

TabulationHashingContext* tabulation_hashing_init(
	uint32_t keyBits, uint32_t hashBits,
	void* (*allocator_proc)(size_t), void (*free_proc)(void*, size_t)
);
void tabulation_hashing_destroy(TabulationHashingContext* ctx);

void tabulation_hashing_set_mm(TabulationHashingContext* ctx, 
	void (*mmwrite)(void* ctx, uint32_t offset, uint32_t value), void* userData);

void tabulation_hashing_set_random_content(TabulationHashingContext* ctx,
	uint32_t(*random_generator_proc)(void*), void* random_generator_user_data);

void tabulation_hashing_hash(TabulationHashingContext* ctx, uint32_t* key, uint32_t* hash);
