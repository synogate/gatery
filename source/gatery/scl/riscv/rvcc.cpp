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
#include "gatery/scl_pch.h"
#include "rvcc.h"

#if __has_include(<external/rvcc/src/defs.c>)
static std::mutex g_rvccLock;
static std::vector<uint32_t> g_code;
static std::vector<std::unique_ptr<char>> g_allocations;

extern "C" 
{
	#include <external/rvcc/src/defs.c>

	extern char* _source;
	extern int _source_idx;
	extern backend_def* _backend;
	extern int _e_code_start;

	void g_initialize();
	void s_load(const char* file);
	void r_initialize_backend(backend_def* be);
	void p_parse();
	void c_generate();

	void c_emit(int instruction)
	{
		g_code.push_back((uint32_t)instruction);
	}

	void e_write_data_string(char* vals, int len)
	{

	}

	void e_add_symbol(char* symbol, int len, int pc)
	{

	}

	void* rvcc_malloc(size_t size)
	{
		g_allocations.emplace_back(new char[size]);
		return g_allocations.back().get();
	}

}

std::vector<uint32_t> gtry::scl::riscv::rvcc(std::string code, uint32_t codeOffset)
{
	std::lock_guard guard{ g_rvccLock };
	g_initialize();

	_e_code_start = (int)codeOffset;
	_source = code.data();
	_source_idx = (int)code.size();

	r_initialize_backend(_backend);
	p_parse();
	c_generate();
	g_allocations.clear();

	std::vector<uint32_t> ret;
	ret.swap(g_code);
	return ret;
}
#endif
