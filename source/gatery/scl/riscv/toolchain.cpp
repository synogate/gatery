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

#include "toolchain.h"

#include <gatery/utils.h>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include <sstream>
#include <iostream>

namespace gtry::scl {

static const char *gcc_march[] = {
	"rv32i"
};
static unsigned gcc_march_len = 1;

static const char *gcc_mabi[] = {
	"ilp32"
};
static unsigned gcc_mabi_len = 1;

void RiscVToolchain::build()
{
	auto path = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
	if (!path.empty())
		boost::filesystem::create_directories(path);

	std::cout << "Using " << path << std::endl;

	HCL_ASSERT(m_compiler == Compiler::GCC);

	HCL_ASSERT_HINT((unsigned)m_arch < gcc_march_len, "Unhandled case!");
	std::string march = gcc_march[(unsigned)m_arch];

	HCL_ASSERT_HINT((unsigned)m_abi < gcc_mabi_len, "Unhandled case!");
	std::string mabi = gcc_mabi[(unsigned)m_abi];

	std::vector<boost::filesystem::path> objectFiles;

	for (auto i : utils::Range(m_codeModules.size())) {
		std::string filename = (boost::format("src_%i") % i).str();

		auto srcFilename = path / (filename + ".cpp");

		{
			std::fstream src(srcFilename.string().c_str(), std::fstream::out);
			src << m_codeModules[i];
		}

		// todo: read back errors ^^
		system((
			boost::format("cd %s && %s -Og -nostdlib -march=%s -mabi=%s -ffreestanding -c %s") % path.string() % m_compilerCmd % march % mabi % srcFilename.string()
		).str().c_str()); //   -ffunction-sections -fdata-sections

		objectFiles.push_back(path / (filename + ".o"));
	}


	auto linkerScriptFilename = path / ("memory_map.ld");
	{
		std::fstream linkerScript(linkerScriptFilename.string().c_str(), std::fstream::out);

		HCL_ASSERT(m_instructionRomAddr < m_dataRamAddr);
		std::uint32_t romSize = m_dataRamAddr - m_instructionRomAddr;
		std::uint32_t ramSize = 0 - m_dataRamAddr;

		linkerScript  << std::hex
			<< "MEMORY {" << std::endl
			<< "	rom (rx) : ORIGIN = 0x" << m_instructionRomAddr << ", LENGTH = 0x" << romSize << std::endl
			<< "	ram (rwx) : ORIGIN = 0x" << m_dataRamAddr << ", LENGTH = 0x" << ramSize << std::endl
			<< "}" << std::endl
			<< "STACK_SIZE = 0x" << m_stackSize << ';' << std::endl;

linkerScript << R"(
SECTIONS {
	. = 0x00000000;
	.text : {
		KEEP(*(.vector*))
		*(.text*)
		_end_text = .;
	} > rom

	.data : {
		_start_data = .;
		*(.rodata*)
		*(.sdata*);
		*(.data*);
		_end_data = .;
	} > ram

	.bss (NOLOAD) : {
		_start_bss = . ;
		*(.bss*)
		*(COMMON)
		_end_bss = . ;
	} > ram

	.stack (NOLOAD) : {
		. = ALIGN(8);
		_start_stack = .;
		. = . + STACK_SIZE;
		. = ALIGN(8);
		_end_stack = .;
	} > ram

	/DISCARD/ : {
		*(.note.gnu.build-id)
		*(.comment)
	}

	_end = . ;
}
		)";
	}


	std::stringstream linkInvocation;
	linkInvocation << "cd " << path.string() << " && " << m_compilerCmd << " -Wl,--gc-sections  -Wl,--print-memory-usage -nostdlib -march=" << march << " -mabi=" << mabi;
	for (auto &f : objectFiles)
		linkInvocation << " \"" << f << '"';

	linkInvocation << " -T memory_map.ld -o linked.elf";
	// todo: read back errors ^^
	system(linkInvocation.str().c_str());	

	system((
		boost::format("cd %s && %s -O binary --only-section=.text linked.elf linked.text") % path.string() % m_objCopyCmd 
	).str().c_str());

	system((
		boost::format("cd %s && %s -O binary --only-section=.data linked.elf linked.data") % path.string() % m_objCopyCmd 
	).str().c_str());

	auto loadFile = [](const boost::filesystem::path &path, std::vector<unsigned char> &dst) {
		std::fstream file(path.string().c_str(), std::fstream::binary | std::fstream::in);
		file.seekg(0, std::fstream::end);
		size_t size = file.tellg();
		file.seekg(0, std::fstream::beg);

		dst.resize(size);
		file.read((char*) dst.data(), dst.size());
	};


	loadFile(path / "linked.text", m_instructionRom);
	loadFile(path / "linked.data", m_dataRom);
}

void RiscVToolchain::addDefaultBootLoader()
{
	m_codeModules.push_back(R"(
typedef unsigned int uint32_t;

extern uint32_t _end_text;
extern uint32_t _start_bss;
extern uint32_t _end_bss;
extern uint32_t _start_data;
extern uint32_t _end_data;
extern uint32_t _start_stack;
extern uint32_t _end_stack;

int main();

__attribute__ ((section(".vectors")))
__attribute__ ((naked))
void vectorTable() {
	asm ("mv zero, zero");
	volatile register uint32_t sp asm ("sp");
	sp = _end_stack;
/*
	uint32_t *init_values_ptr = &_end_text;
	uint32_t *data_ptr = &_start_data;

	if (init_values_ptr != data_ptr)
		for (; data_ptr < &_end_data;)
			*data_ptr++ = *init_values_ptr++;

	for (uint32_t *bss_ptr = &_start_bss; bss_ptr < &_end_bss;)
		*bss_ptr++ = 0;

*/

	main();

	while (true);
}

)");
}

std::string RiscVToolchain::getDisassembly()
{
	return "";
}

}
