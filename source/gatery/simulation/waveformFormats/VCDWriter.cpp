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
#include "gatery/pch.h"
#include "VCDWriter.h"
#include "../../utils/Range.h"

#include <chrono>

#ifdef _WIN32
#include <ktmw32.h>
#include <io.h>
#pragma comment(lib, "KtmW32.lib")

#define USE_TRANSACTION
#endif

gtry::sim::VCDWriter::VCDWriter(std::string filename) :
	m_FileName(filename)
{
	auto parentPath = std::filesystem::path(filename).parent_path();
	if (!parentPath.empty())
		std::filesystem::create_directories(parentPath);
	openFile(true);

	auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	tm now_tb;
#ifdef WIN32
	localtime_s(&now_tb, &now);
#else
	localtime_r(&now, &now_tb);
#endif

	m_File
		<< "$date\n" << std::put_time(&now_tb, "%Y-%m-%d %X") << "\n$end\n"
		<< "$version\nGatery simulation output\n$end\n"
		<< "$timescale\n1ps\n$end\n";
}

gtry::sim::VCDWriter::~VCDWriter()
{
#ifdef USE_TRANSACTION
	m_File.close();
	if (CommitTransaction(m_Transaction) == FALSE)
		std::cerr << "transaction failed\n";
	CloseHandle(m_Transaction);
#endif
}

bool gtry::sim::VCDWriter::commit()
{
#ifdef USE_TRANSACTION
	m_File.close();
	if (CommitTransaction(m_Transaction) == FALSE)
		std::cerr << "transaction failed\n";
	CloseHandle(m_Transaction);

	openFile(false);
#endif
	return true;
}

gtry::sim::VCDWriter::Scope gtry::sim::VCDWriter::beginModule(std::string_view name)
{
	assert(!name.empty());
	assert(!m_EndDefinitions);
	m_File << "$scope module " << name << " $end\n";

	return Scope([&]() {
		assert(!m_EndDefinitions);
		m_File << "$upscope $end\n";
	});
}

void gtry::sim::VCDWriter::declareWire(size_t width, std::string_view code, std::string_view label)
{
	assert(!m_EndDefinitions);
	m_File << "$var wire " << width << " " << code << " " << label << " $end\n";
}

void gtry::sim::VCDWriter::declareReal(std::string_view code, std::string_view label)
{
	assert(!m_EndDefinitions);
	m_File << "$var real 0 " << code << " " << label << " $end\n";
}

void gtry::sim::VCDWriter::declareString(std::string_view code, std::string_view label)
{
	assert(!m_EndDefinitions);
	m_File << "$var string 0 " << code << " " << label << " $end\n";
}

gtry::sim::VCDWriter::Scope gtry::sim::VCDWriter::beginDumpVars()
{
	assert(!m_EndDefinitions);
	m_File
		<< "$enddefinitions $end\n"
		<< "$dumpvars\n";
	m_EndDefinitions = true;

	return Scope([&]() {
		m_File << "$end\n";
	});
}

void gtry::sim::VCDWriter::writeState(std::string_view code, const DefaultBitVectorState& state, size_t offset, size_t size)
{
	assert(m_EndDefinitions);

	m_File << 'b';
	for(auto i : utils::Range(size)) {
		auto bitIdx = size - 1 - i;
		bool def = state.get(DefaultConfig::DEFINED, offset + bitIdx);
		bool val = state.get(DefaultConfig::VALUE, offset + bitIdx);
		if(!def)
			m_File << 'X';
		else if(val)
			m_File << '1';
		else
			m_File << '0';
	}
	m_File << ' ' << code << '\n';
}

void gtry::sim::VCDWriter::writeState(std::string_view code, size_t size, uint64_t defined, uint64_t valid)
{
	assert(m_EndDefinitions);

	m_File << 'b';
	for(auto i : utils::Range(size)) {
		auto bitIdx = size - 1 - i;
		bool def = (defined >> bitIdx) & 1;
		bool val = (valid >> bitIdx) & 1;
		if(!def)
			m_File << 'X';
		else if(val)
			m_File << '1';
		else
			m_File << '0';
	}
	m_File << ' ' << code << '\n';
}

void gtry::sim::VCDWriter::writeString(std::string_view code, size_t size, std::string_view text)
{
	assert(m_EndDefinitions);
	
	m_File << 'b';
	for(size_t i = 0; i < size / 8 && i < text.size(); ++i)
	{
		int c = text[i];
		for(size_t i = 0; i < 8; ++i)
		{
			m_File << ((c & 0x80) ? '1' : '0');
			c <<= 1;
		}
	}
//	for(size_t i = text.size(); i < size; ++i)
//		m_File << "00";

	m_File << ' ' << code << '\n';
}

void gtry::sim::VCDWriter::writeString(std::string_view code, std::string_view text)
{
	assert(m_EndDefinitions);
	
	if (text.empty())
		text = " "; // empty string is not supported by surfer. space is also not supported, but it's better than nothing

	m_File << 's';
	for (auto c : text)
		if (c == ' ')
			m_File << "\\x20";
		else
			m_File << c;

	m_File << ' ' << code << '\n';
}

void gtry::sim::VCDWriter::writeBitState(std::string_view code, bool defined, bool value)
{
	assert(m_EndDefinitions);

	if(!defined)
		m_File << 'X';
	else if(value)
		m_File << '1';
	else
		m_File << '0';

	m_File << code << '\n';
}

void gtry::sim::VCDWriter::writeTime(size_t time)
{
	assert(m_EndDefinitions);
	m_File << '#' << time << '\n';
}

void gtry::sim::VCDWriter::openFile(bool createEmpty)
{
#ifdef USE_TRANSACTION
	m_Transaction = CreateTransaction(NULL, NULL, TRANSACTION_DO_NOT_PROMOTE, 0, 0, 0, NULL);

	DWORD mode = createEmpty ? CREATE_ALWAYS : OPEN_EXISTING;
	USHORT transactionReadMode = 0xFFFF;
	HANDLE file_handle = CreateFileTransactedA(m_FileName.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, NULL, mode, FILE_ATTRIBUTE_NORMAL, NULL, 
		m_Transaction, &transactionReadMode, NULL);
	if (file_handle == INVALID_HANDLE_VALUE)
		throw std::runtime_error("Could not open vcd file for writing! " + m_FileName);

	int file_descriptor = _open_osfhandle((intptr_t)file_handle, 0);
	FILE* file = _fdopen(file_descriptor, "a");
	m_File = std::ofstream{ file };
#else
	m_File.open(m_FileName.c_str(), std::ofstream::binary);

	if (!m_File)
		throw std::runtime_error("Could not open vcd file for writing! " + m_FileName);
#endif
}
