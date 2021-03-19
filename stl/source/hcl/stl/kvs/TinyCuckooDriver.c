#include "TinyCuckooDriver.h"

#include <string.h>

#define HASH_WORDS_LIMIT 16

static uint32_t Log2(uint32_t v)
{
	uint32_t ret = 0;
	while (v >>= 1)
		++ret;
	return ret;
}

static uint32_t Log2C(uint32_t v)
{
	if (v == 1)
		return 0;
	return Log2(v - 1) + 1;
}

static uint32_t extract_bit_range(uint32_t* field, uint32_t index, uint32_t elementWidth)
{
	uint32_t offset = index * elementWidth;

	uint32_t low = field[offset / 32];
	uint32_t high = field[offset / 32 + 1];

	uint32_t ret = 0;
	ret |= low >> (offset % 32);
	if(offset % 32 != 0)
		ret |= high << (32 - offset % 32);
	ret &= (1ull << elementWidth) - 1;
	return ret;
}

TinyCuckooContext* tiny_cuckoo_init(
	uint32_t capacity, uint32_t numTables,
	uint32_t keyWidth, size_t valueWidth,
	void* (*allocator_proc)(size_t size), void (*free_proc)(void*, size_t)
)
{
	size_t keyWords = (keyWidth + 31) / 32;
	size_t valueWords = (valueWidth + 31) / 32;
	size_t itemWords = 1 + keyWords + valueWords;
	size_t tableWords = itemWords * capacity;
	TinyCuckooContext* ctx;

	if (capacity % numTables)
		return NULL;

	ctx = allocator_proc(sizeof(TinyCuckooContext) + tableWords * 4);
	if (!ctx)
		return NULL;

	memset(ctx, 0, sizeof(TinyCuckooContext) + tableWords * 4);
	ctx->alloc = allocator_proc;
	ctx->free = free_proc;
	ctx->hash = NULL;
	ctx->hashCtx = 0;

	ctx->capacity = capacity;
	ctx->numTables = numTables;
	ctx->keyWords = (uint16_t)keyWords;
	ctx->valueWords = (uint16_t)valueWords;
	ctx->itemWords = (uint16_t)itemWords;

	ctx->limitChainDepth = capacity / numTables;
	ctx->limitChainJobs = capacity * 2;

	ctx->hashBitPerTable = Log2C(ctx->capacity / ctx->numTables);
	ctx->hashWords = (ctx->hashBitPerTable * ctx->numTables + 31) / 32;

	if (ctx->hashWords > HASH_WORDS_LIMIT)
	{
		free_proc(ctx, sizeof(TinyCuckooContext) + tableWords * 4);
		return NULL;
	}

	return ctx;
}

void tiny_cuckoo_set_hash(TinyCuckooContext* ctx, void (*hash_proc)(void*, uint32_t*, uint32_t*), void* userData)
{
	ctx->hash = hash_proc;
	ctx->hashCtx = userData;
}

static TinyCuckooItem* tiny_cuckoo_item(TinyCuckooContext* ctx, uint32_t table, uint32_t* hash)
{
	size_t index;
	index = extract_bit_range(hash, table, ctx->hashBitPerTable);
	index += table * ctx->capacity / ctx->numTables;
	return (TinyCuckooItem*)(ctx->items + index * ctx->itemWords);
}

static TinyCuckooItem* tiny_cuckoo_find(TinyCuckooContext* ctx, uint32_t* key)
{
	uint32_t hash[HASH_WORDS_LIMIT + 1];
	uint32_t i;
	TinyCuckooItem* item;

	if (!ctx->hash)
		return NULL;

	ctx->hash(ctx->hashCtx, key, hash);

	for (i = 0; i < ctx->numTables; ++i)
	{
		item = tiny_cuckoo_item(ctx, i, hash);
		
		if(item->valid)
			if (!memcmp(item->key, key, ctx->keyWords * 4ull))
				return item;
	}
	return NULL;
}

struct TinyCuckooMove
{
	TinyCuckooItem* item;
	size_t depth;
	struct TinyCuckooMove* parent;
	struct TinyCuckooMove* next;
	struct TinyCuckooMove* prev;
	struct TinyCuckooMove* next_job_list;
};

typedef struct {
	TinyCuckooContext* ctx;
	struct TinyCuckooMove front, back;
	struct TinyCuckooMove* job_list;
	size_t job_list_size;
} TinyCuckooUpdate;

static void insert_after(struct TinyCuckooMove* new_item, struct TinyCuckooMove* after)
{
	new_item->next = after->next;
	new_item->prev = after;
	after->next->prev = new_item;
	after->next = new_item;
}

static int tiny_cuckoo_move_slot_step(TinyCuckooUpdate* uctx)
{
	uint32_t hash[HASH_WORDS_LIMIT + 1];
	uint32_t i;
	TinyCuckooItem* item;
	struct TinyCuckooMove* new_job;
	struct TinyCuckooMove* job = uctx->back.prev;
	uctx->back.prev = job->prev;
	
	uctx->ctx->hash(uctx->ctx->hashCtx, job->item->key, hash);

	for (i = 0; i < uctx->ctx->numTables; ++i)
	{
		item = tiny_cuckoo_item(uctx->ctx, i, hash);
		if (item == job->item)
			continue;

		new_job = uctx->ctx->alloc(sizeof(struct TinyCuckooMove));
		new_job->item = item;
		new_job->depth = job->depth + 1;
		new_job->parent = job;
		new_job->next_job_list = uctx->job_list;
		uctx->job_list = new_job;
		uctx->job_list_size++;
		insert_after(new_job, &uctx->front);

		if (!item->valid)
			return 1; // found valid cuckoo chain
	}
	return 0;
}

static TinyCuckooItem* tiny_cuckoo_walk_chain(TinyCuckooContext* ctx, struct TinyCuckooMove* mv)
{
	while (mv->parent)
	{
		memcpy(mv->item, mv->parent->item, ctx->itemWords * 4ull);
		mv = mv->parent;
	}
	return mv->item;
}

static void tiny_cuckoo_free_jobs(TinyCuckooUpdate* uctx)
{
	struct TinyCuckooMove* job, *job_next;

	for (job = uctx->job_list; job; job = job_next)
	{
		job_next = job->next_job_list;
		uctx->ctx->free(job, sizeof(struct TinyCuckooMove));
	}
}

static int tiny_cuckoo_update_existing(TinyCuckooContext* ctx, uint32_t* key, uint32_t* value, uint32_t* hash)
{
	uint32_t i;
	TinyCuckooItem* item;

	for (i = 0; i < ctx->numTables; ++i)
	{
		item = tiny_cuckoo_item(ctx, i, hash);

		if (item->valid)
			if (!memcmp(item->key, key, ctx->keyWords * 4ull))
			{
				memcpy(item->key + ctx->keyWords, value, ctx->valueWords * 4ull);
				return 1;
			}
	}
	return 0;
}

static int tiny_cuckoo_update_insert_if_free(TinyCuckooContext* ctx, uint32_t* key, uint32_t* value, uint32_t* hash)
{
	uint32_t i;
	TinyCuckooItem* item;

	for (i = 0; i < ctx->numTables; ++i)
	{
		item = tiny_cuckoo_item(ctx, i, hash);
		if (!item->valid)
		{
			item->valid = 1;
			memcpy(item->key, key, ctx->keyWords * 4ull);
			memcpy(item->key + ctx->keyWords, value, ctx->valueWords * 4ull);
			return 1;
		}
	}
	return 0;
}

static int tiny_cuckoo_update_insert_by_moving(TinyCuckooContext* ctx, uint32_t* key, uint32_t* value, uint32_t* hash)
{
	int ret = 0;
	uint32_t i;
	TinyCuckooItem* item;
	TinyCuckooUpdate uctx;
	struct TinyCuckooMove* job;

	memset(&uctx, 0, sizeof(uctx));
	uctx.ctx = ctx;
	uctx.front.next = &uctx.back;
	uctx.back.prev = &uctx.front;

	for (i = 0; i < ctx->numTables; ++i)
	{
		job = ctx->alloc(sizeof(struct TinyCuckooMove));
		job->item = tiny_cuckoo_item(ctx, i, hash);
		job->depth = 0;
		job->parent = NULL;
		job->next_job_list = uctx.job_list;
		uctx.job_list = job;
		insert_after(job, &uctx.front);
	}

	while (!tiny_cuckoo_move_slot_step(&uctx))
	{
		if (uctx.job_list_size >= ctx->limitChainJobs)
			break;
		if (uctx.back.prev->depth >= ctx->limitChainDepth)
			break;
	}

	job = uctx.front.next;
	if (!job->item->valid)
	{
		item = tiny_cuckoo_walk_chain(ctx, job);
		ret = (int)uctx.job_list_size;

		item->valid = 1;
		memcpy(item->key, key, ctx->keyWords * 4ull);
		memcpy(item->key + ctx->keyWords, value, ctx->valueWords * 4ull);
	}

	tiny_cuckoo_free_jobs(&uctx);
	return ret;
}

int tiny_cuckoo_update(TinyCuckooContext* ctx, uint32_t* key, uint32_t* value)
{
	uint32_t hash[HASH_WORDS_LIMIT + 1];

	if (!ctx->hash)
		return 0;

	ctx->hash(ctx->hashCtx, key, hash);

	if (tiny_cuckoo_update_existing(ctx, key, value, hash))
		return 1;

	if (tiny_cuckoo_update_insert_if_free(ctx, key, value, hash))
		return 2;

	if (tiny_cuckoo_update_insert_by_moving(ctx, key, value, hash))
		return 3;

	return 0;
}

uint32_t* tiny_cuckoo_lookup(TinyCuckooContext* ctx, uint32_t* key)
{
	TinyCuckooItem* item = tiny_cuckoo_find(ctx, key);
	if (!item)
		return NULL;
	return &item->key[ctx->keyWords];
}

int tiny_cuckoo_remove(TinyCuckooContext* ctx, uint32_t* key)
{
	TinyCuckooItem* item = tiny_cuckoo_find(ctx, key);
	if (item)
	{
		item->valid = 0;
		return 1;
	}
	return 0;
}

void tiny_cuckoo_destroy(TinyCuckooContext* ctx)
{
	uint32_t tableWords = ctx->itemWords * ctx->capacity;
	ctx->free(ctx, sizeof(TinyCuckooContext) + tableWords * 4ull);
}

