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

void* tiny_cuckoo_iterate(TinyCuckooContext* ctx, void* iterator, uint32_t* key, uint32_t* value);
