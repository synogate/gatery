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
#include <filesystem>
#include <span>

#include <boost/iostreams/device/mapped_file.hpp>

#include <gatery/frontend.h>
#include <gatery/simulation/BitVectorState.h>

namespace gtry::scl::riscv
{
	enum class Bitness
	{
		rv32, rv64
	};

	enum class Endianness
	{
		little, big
	};

	class ElfLoader
	{
	public:
		struct Section
		{
			std::span<const uint8_t> data;
			BitWidth size;
			size_t flags;
			uint64_t offset;
			uint64_t alignment;
		};

		struct MegaSection
		{
			uint64_t offset = 0;
			BitWidth size;
			std::vector<const Section*> subSections;

			sim::DefaultBitVectorState memoryState() const;
		};

	public:
		ElfLoader(const std::filesystem::path& file) { load(file); }
		void load(const std::filesystem::path& file);

		Bitness bitness() const;
		Endianness endianness() const;
		uint64_t entryPoint() const;

		const std::vector<Section>& sections() const { return m_ProgramSections; }
		MegaSection sections(size_t allOfFlags, size_t anyOfFlags, size_t excludeFlags) const;


	protected:
		template<typename T>
		T load(size_t offset) const;

	private:
		boost::iostreams::mapped_file_source m_Binary;
		std::vector<Section> m_ProgramSections;
	};
}
