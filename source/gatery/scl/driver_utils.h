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
#include <random>
#include <vector>

struct MmTestCtx
{
	std::vector<uint32_t> mem;
};

static void MmTestWrite(void* ctx, uint32_t offset, uint32_t value)
{
	auto* c = (MmTestCtx*)ctx;
	if (offset >= c->mem.size())
		c->mem.resize(offset + 1);
	c->mem[offset] = value;
}

static void* driver_alloc(size_t s)
{
	return malloc(s);
}

static void driver_free(void* ptr, size_t)
{
	free(ptr);
}

static void driver_basic_hash(void* ctx, uint32_t* key, uint32_t* hash)
{
	const uint32_t k = *key;
	hash[0] = k * 609598081u;
	hash[1] = k * 1067102063u;
	hash[2] = k * 190989923u;
	hash[3] = k * 905010023u;
	hash[4] = k * 2370688493u;
	hash[5] = k * 3059132147u;
	hash[6] = k * 1500458227u;
	hash[7] = k * 1781057147u;
}

static uint32_t driver_random_generator(void* mt)
{
	auto& rng = *(std::mt19937*)mt;
	return rng();
}
