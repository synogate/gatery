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

#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void c_emit(int instruction);
void e_write_data_string(char* vals, int len);
void e_add_symbol(char* symbol, int len, int pc);
void* rvcc_malloc(size_t size);

#if __has_include(<external/rvcc/src/defs.c>)

#define malloc rvcc_malloc

#pragma warning (disable : 4267)
#include <external/rvcc/src/defs.c>
#include <external/rvcc/src/globals.c>
#include <external/rvcc/src/helpers.c>
#include <external/rvcc/src/lexer.c>
#include <external/rvcc/src/source.c>
#include <external/rvcc/src/arch/riscv.c>
#include <external/rvcc/src/parser.c>
#include <external/rvcc/src/codegen.c>

#endif
