/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "TabulationHashingDriver.h"

#include <string.h>

#define SYM_WIDTH 8
#define SYM_COUNT (1 << SYM_WIDTH)

static uint32_t numSymbols(uint32_t val, uint32_t symbolWidth)
{
	return (val + symbolWidth - 1) / symbolWidth;
}

TabulationHashingContext* tabulation_hashing_init(
	uint32_t keyBits, uint32_t hashBits, 
	void* (*allocator_proc)(size_t), void(*free_proc)(void*, size_t))
{
	uint32_t tableWords = numSymbols(hashBits, 32) * SYM_COUNT;
	uint32_t numTables = numSymbols(keyBits, SYM_WIDTH);
	uint32_t tablesBytes = numTables * tableWords * 4;
	TabulationHashingContext* ctx;

	ctx = allocator_proc(sizeof(TabulationHashingContext) + tablesBytes);
	if (!ctx)
		return NULL;

	memset(ctx, 0, sizeof(TabulationHashingContext) + tablesBytes);
	ctx->alloc = allocator_proc;
	ctx->free = free_proc;

	ctx->keyWidth = keyBits;
	ctx->hashWidth = hashBits;

	ctx->mmCtx = NULL;
	ctx->mmwrite = NULL;

	return ctx;
}

void tabulation_hashing_destroy(TabulationHashingContext* ctx)
{
	uint32_t tableWords = numSymbols(ctx->hashWidth, 32) * SYM_COUNT;
	uint32_t numTables = numSymbols(ctx->keyWidth, SYM_WIDTH);
	uint32_t tablesBytes = numTables * tableWords * 4;

	ctx->free(ctx, sizeof(TabulationHashingContext) + tablesBytes);
}

void tabulation_hashing_set_mm(TabulationHashingContext* ctx, 
	void(*mmwrite)(void* ctx, uint32_t offset, uint32_t value), void* userData)
{
	ctx->mmCtx = userData;
	ctx->mmwrite = mmwrite;
}

void tabulation_hashing_set_random_content(TabulationHashingContext* ctx, 
	uint32_t(*random_generator_proc)(void*), void* random_generator_user_data)
{
	uint32_t slotWords = numSymbols(ctx->hashWidth, 32);
	uint32_t numTables = numSymbols(ctx->keyWidth, SYM_WIDTH);
	uint32_t numSymbols = SYM_COUNT;
	uint32_t t, s, i, offset;
	uint32_t* slot;

	for (t = 0; t < numTables; ++t)
	{
		if (t + 1 == numTables && ctx->keyWidth % 8 != 0)
		{
			// limit the number of table entries on last table
			// the hw would otherwise ignore higher address bits and thus overwrite
			numSymbols = 1 << (ctx->keyWidth % 8);
		}

		for (s = 0; s < numSymbols; ++s)
		{
			offset = (t * SYM_COUNT + s) * slotWords;
			slot = ctx->items + offset;
			for (i = 0; i < slotWords; ++i)
				slot[i] = random_generator_proc(random_generator_user_data);

			if (ctx->mmwrite)
			{
				for (i = 0; i < slotWords; ++i)
					ctx->mmwrite(ctx->mmCtx, 1 + i, slot[i]);
				ctx->mmwrite(ctx->mmCtx, 0, offset / slotWords);
			}
		}

	}
}

void tabulation_hashing_hash(TabulationHashingContext* ctx, uint32_t* key, uint32_t* hash)
{
	uint32_t slotWords = numSymbols(ctx->hashWidth, 32);
	uint32_t numTables = numSymbols(ctx->keyWidth, SYM_WIDTH);
	uint32_t t, i, offset;
	uint32_t* slot;
	uint8_t* keyBytes = (uint8_t*)key;

	for (i = 0; i < slotWords; ++i)
		hash[i] = 0;

	for (t = 0; t < numTables; ++t)
	{
		offset = (t * SYM_COUNT + keyBytes[t]) * slotWords;
		slot = ctx->items + offset;

		for (i = 0; i < slotWords; ++i)
			hash[i] ^= slot[i];
	}

	if (ctx->hashWidth % 32 != 0)
	{
		hash[ctx->hashWidth / 32] &= (1 << ctx->hashWidth % 32) - 1;
	}
}
