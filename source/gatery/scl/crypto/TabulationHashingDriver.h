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
