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
#include "ElfLoader.h"

using namespace gtry::scl::riscv;

template<typename T>
inline T ElfLoader::load(size_t offset) const
{
	if (offset + sizeof(T) <= m_Binary.size())
		return *(const T*)(m_Binary.data() + offset);
	throw std::runtime_error{ "elf offset is out of range" };
}

void ElfLoader::load(const std::filesystem::path& file)
{
	m_Binary.open(file.string());
	//const char* ptr = m_Binary.data();

	if (load<uint32_t>(0) != 0x464c457f)
		throw std::runtime_error{ "elf magic missmatch" };

	if (load<uint16_t>(16) != 2)
		throw std::runtime_error{ "elf is not an executable" };

	if (load<uint16_t>(18) != 0xF3)
		throw std::runtime_error{ "elf instruction set is not RISC-V" };

	HCL_ASSERT_HINT(bitness() == Bitness::rv32, "64bit program header not yet implemented");

	loadProgramHeader();
	loadSectionHeader();
}

Bitness ElfLoader::bitness() const
{
	switch (load<uint8_t>(4))
	{
	case 1: return Bitness::rv32;
	case 2: return Bitness::rv64;
	default: 
		HCL_ASSERT(false);
	}
}

uint64_t gtry::scl::riscv::ElfLoader::entryPoint() const
{
	return load<uint32_t>(0x18);
}

void gtry::scl::riscv::ElfLoader::splitTextAndRoData()
{
	for (size_t i = 0; i < m_ProgramSegments.size(); ++i)
	{
		Segment& seg = m_ProgramSegments[i];
		if (!(seg.flags & 1)) // filter non executable segments
			continue;

		std::vector<Section> executable;
		std::vector<Section> data;

		for (auto& it_sec : m_Sections)
		{
			const Section& sec = it_sec.second;
			if (!(sec.flags & 2)) // filter non allocation sections
				continue;

			if (sec.offset >= seg.offset && sec.offset < seg.offset + seg.size.bytes())
			{
				if ((sec.flags & 4)) // executable sections
					executable.push_back(sec);
				else
					data.push_back(sec);
			}
		}



		Segment rodata = seg;
		{ // remove executable sections from rodata segment
			rodata.flags &= ~1; // remove execute flag

			std::sort(executable.begin(), executable.end(), [](auto& a, auto& b) { return a.offset < b.offset; });
			for(auto& sec : executable)
				if (sec.offset == rodata.offset) // MiO: will break sometimes when segment contains data that is not in any section
				{
					rodata.offset += sec.data.size();
					rodata.size = rodata.size - sec.data.size() * 8;
					rodata.data = rodata.data.subspan(sec.data.size());
				}

		}

		{ // remove non executable sections from text segment
			std::sort(data.begin(), data.end(), [](auto& a, auto& b) { return a.offset > b.offset; });
			for (auto& sec : data)
				if (sec.offset + sec.data.size() == seg.offset + seg.size.bytes())
				{
					seg.size = seg.size - sec.data.size() * 8;
					seg.data = seg.data.subspan(0, seg.size.bytes());
				}
		}

		if(rodata.size)
			m_ProgramSegments.push_back(rodata);
	}
}

Endianness ElfLoader::endianness() const
{
	switch (load<uint8_t>(5))
	{
	case 1: return Endianness::little;
	case 2: return Endianness::big;
	default:
		HCL_ASSERT(false);
	}
}

ElfLoader::MegaSegment ElfLoader::segments(size_t allOfFlags, size_t anyOfFlags, size_t excludeFlags) const
{
	MegaSegment ret;

	size_t sectionBegin = std::numeric_limits<size_t>::max();
	size_t sectionEnd = 0;

	for (const Segment& s : m_ProgramSegments)
	{
		bool allOf = (s.flags & allOfFlags) == allOfFlags;
		bool anyOf = (s.flags & anyOfFlags) != 0;
		bool exclude = (s.flags & excludeFlags) != 0;
		if (!exclude && (allOf || anyOf))
		{
			sectionBegin = std::min<size_t>(sectionBegin, s.offset);
			sectionEnd = std::max<size_t>(sectionEnd, s.offset + s.size.bytes());
			ret.subSections.push_back(&s);
		}
	}

	if (!ret.subSections.empty())
	{
		ret.offset = sectionBegin;
		ret.size = BitWidth{ (sectionEnd - sectionBegin) * 8 };
	}
	return ret;
}

const ElfLoader::Section* ElfLoader::section(std::string_view name) const
{
	auto it = m_Sections.find(name);
	if (it != m_Sections.end())
		return &it->second;
	return nullptr;
}

void gtry::scl::riscv::ElfLoader::loadProgramHeader()
{
	const uint32_t program_header_offset = load<uint32_t>(28);
	const uint16_t program_header_entry_size = load<uint16_t>(42);
	const size_t program_header_count = load<uint16_t>(44);

	for (size_t i = 0; i < program_header_count; ++i)
	{
		const size_t pos = program_header_offset + i * program_header_entry_size;
		const uint32_t type = load<uint32_t>(pos);
		HCL_ASSERT_HINT(type != 2, "dynamic linking is not implemented");
		HCL_ASSERT_HINT(type != 3, "dynamic loader is not supported");

		if (type == 1)
		{
			size_t data_offset = load<uint32_t>(pos + 4);
			size_t data_size = load<uint32_t>(pos + 16);
			size_t skip_bytes = 0;

			if (data_offset == 0)
			{
				// we dont really need the elf header, so we discard it
				skip_bytes = program_header_offset + program_header_count * program_header_entry_size;
				HCL_ASSERT(skip_bytes < data_size);
			}

			auto* data_ptr = (const uint8_t*)m_Binary.data() + data_offset + skip_bytes;

			Segment s;
			s.data = std::span(data_ptr, data_size - skip_bytes);
			s.offset = load<uint32_t>(pos + 8) + skip_bytes;
			s.size = BitWidth{ (load<uint32_t>(pos + 20) - skip_bytes) * 8 };
			s.flags = load<uint32_t>(pos + 24);
			s.alignment = load<uint32_t>(pos + 28);
			m_ProgramSegments.push_back(s);
		}
	}
}

void gtry::scl::riscv::ElfLoader::loadSectionHeader()
{
	const size_t section_header_offset = load<uint32_t>(0x20);
	const size_t section_header_entry_size = load<uint16_t>(0x2E);
	const size_t section_header_count = load<uint16_t>(0x30);

	const size_t string_header_offset = load<uint16_t>(0x32);
	const size_t string_table = load<uint32_t>(section_header_offset + string_header_offset * section_header_entry_size + 0x10);

	for (size_t i = 0; i < section_header_count; ++i)
	{
		const size_t pos = section_header_offset + i * section_header_entry_size;

		const size_t name_offset = load<uint32_t>(pos + 0);
		const uint32_t data_offset = load<uint32_t>(pos + 0x10);
		const uint32_t data_size = load<uint32_t>(pos + 0x14);

		Section s;
		s.name = m_Binary.data() + string_table + name_offset;
		s.type = load<uint32_t>(pos + 4);
		s.flags = load<uint32_t>(pos + 8);
		s.offset = load<uint32_t>(pos + 0x0c);
		s.data = std::span((const uint8_t*)m_Binary.data() + data_offset, data_size);
		m_Sections[s.name] = s;
	}
}

gtry::sim::DefaultBitVectorState gtry::scl::riscv::ElfLoader::MegaSegment::memoryState() const
{
	sim::DefaultBitVectorState ret;
	ret.resize(size.bits());
	ret.clearRange(sim::DefaultConfig::DEFINED, 0, size.bits());

	for (const Segment* s : subSections)
	{
		auto sectionState = sim::createDefaultBitVectorState(s->data.size() * 8, s->data.data());
		ret.copyRange((s->offset - offset) * 8, sectionState, 0, sectionState.size());

		if (s->data.size() != s->size.bytes())
		{
			size_t zeroOffset = s->offset - offset + s->data.size();
			size_t zeroSize = s->size.bytes() - s->data.size();

			ret.setRange(sim::DefaultConfig::DEFINED, zeroOffset * 8, zeroSize * 8);
			ret.clearRange(sim::DefaultConfig::VALUE, zeroOffset * 8, zeroSize * 8);
		}
	}
	return ret;
}
