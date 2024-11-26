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
#include <string_view>
#include <span>
#include <filesystem>
#include <boost/iostreams/device/mapped_file.hpp>

namespace gtry::scl
{
	/// <summary>
	/// VHDXReader can be used to read VHDX disk image files. 
	/// It is meant for circuit testing and therefore does not handle corrupted files as well as the spec suggests.
	/// Fixed and Dynamic images are supported. Differential images are not yet implemented.
	/// 
	/// block - In VHDX terms is the minimal allocation unit and is at least 1 MiB in size.
	/// sector - Is the 512 or 4096 byte unit known from disks. 
	/// </summary>
	class VHDXReader
	{
	public:
		struct MetaInfo
		{
			// size units are in bytes
			uint32_t logicalSectorSize = 0;
			uint32_t physicalSectorSize = 0;
			uint32_t blockSize = 0;
			uint32_t chunkRatio = 0;
			uint32_t sectorsPerBlock = 0;
			uint64_t diskSize = 0;
			uint64_t diskId[2] = {};
		};

		VHDXReader() = default;
		VHDXReader(const std::filesystem::path& _vhdxFile)
		{
			open(_vhdxFile);
		}

		void open(const std::filesystem::path& _vhdxFile)
		{
			struct RegionEntry
			{
				uint64_t guid[2];
				uint64_t offset;
				uint32_t size;
				uint32_t reserved;
			};

			struct MetadataEntry
			{
				uint64_t guid[2];
				uint32_t offset;
				uint32_t size;
				uint32_t flags;
				uint32_t reserved;
			};

			m_mm.open(_vhdxFile.string());
			std::string_view data(m_mm.data(), m_mm.size());
			if (!data.starts_with("vhdxfile"))
				throw std::runtime_error("vhdx magic missmatch");

			std::string_view regionHdr = data.substr(192 * 1024);
			if (!regionHdr.starts_with("regi"))
				throw std::runtime_error("vhdx region header magic missmatch");
			uint32_t numRegionEntries = *(uint32_t*)(regionHdr.data() + 8);
			if (numRegionEntries > 128)
				throw std::runtime_error("unexpected number of region table entries");

			const RegionEntry* regent = (const RegionEntry*)(regionHdr.data() + 16);
			const RegionEntry* regentBAT = nullptr;
			const RegionEntry* regentMetadata = nullptr;
			for (size_t i = 0; i < numRegionEntries; ++i)
			{
				if (regent[i].offset + regent[i].size > m_mm.size())
					throw std::runtime_error("region entry out of bounds");
				if (regent[i].guid[0] == 0x4200f6232dc27766 &&
					regent[i].guid[1] == 0x084afd9b5e11649d)
					regentBAT = regent + i;
				if (regent[i].guid[0] == 0x4b9a47908b7ca206 &&
					regent[i].guid[1] == 0x6e880f055f57feb8)
					regentMetadata = regent + i;
			}
			if (!regentBAT)
				throw std::runtime_error("BAT not found in region table");
			if (!regentMetadata)
				throw std::runtime_error("Metadata not found in region table");

			std::string_view metadataHead = data.substr(regentMetadata->offset);
			if (!metadataHead.starts_with("metadata"))
				throw std::runtime_error("Metadata magic missmatch");

			uint16_t metadataCount = *(const uint16_t*)(metadataHead.data() + 10);
			if (metadataCount > 2047)
				throw std::runtime_error("Metadata count invalid");

			const MetadataEntry* metaentry = (const MetadataEntry*)(metadataHead.data() + 32);
			for (uint16_t m = 0; m < metadataCount; ++m)
			{
				if (regentMetadata->offset + metaentry[m].offset + 16 > m_mm.size())
					throw std::runtime_error("metadata out of bounds");

				if (metaentry[m].guid[0] == 0x4d43fa36caa16737 &&
					metaentry[m].guid[1] == 0x6be744aaf033b6b3)
					m_meta.blockSize = *(const uint32_t*)(metadataHead.data() + metaentry[m].offset);
				if (metaentry[m].guid[0] == 0x4876cd1b2fa54224 &&
					metaentry[m].guid[1] == 0xb8f43bd8be5d11b2)
					m_meta.diskSize = *(const uint64_t*)(metadataHead.data() + metaentry[m].offset);
				if (metaentry[m].guid[0] == 0x4709a96f8141bf1d &&
					metaentry[m].guid[1] == 0x5fabfaa833f247ba)
					m_meta.logicalSectorSize = *(const uint32_t*)(metadataHead.data() + metaentry[m].offset);
				if (metaentry[m].guid[0] == 0x4471445dcda348c7 &&
					metaentry[m].guid[1] == 0x56c5515288e9c99c)
					m_meta.physicalSectorSize = *(const uint32_t*)(metadataHead.data() + metaentry[m].offset);
				if (metaentry[m].guid[0] == 0x4523b2e6beca12ab &&
					metaentry[m].guid[1] == 0x46c700e009c3ef93)
				{
					m_meta.diskId[0] = *(const uint64_t*)(metadataHead.data() + metaentry[m].offset);
					m_meta.diskId[1] = *(const uint64_t*)(metadataHead.data() + metaentry[m].offset + 8);
				}
			}
			m_meta.chunkRatio = uint64_t(1 << 23) * m_meta.logicalSectorSize / m_meta.blockSize;
			m_meta.sectorsPerBlock = m_meta.blockSize / m_meta.logicalSectorSize;

			m_BAT = std::span<const uint64_t>(
				(const uint64_t*)(m_mm.data() + regentBAT->offset),
				regentBAT->size / 8
			);
		}

		std::span<const char> block(size_t _index) const
		{
			_index += _index / m_meta.chunkRatio; // skip sector block entries

			if (_index >= m_BAT.size())
				throw std::runtime_error("block index out of range");

			if ((m_BAT[_index] & 0x7) != 6)
				throw std::runtime_error("block not present");

			size_t blockoffset = (m_BAT[_index] >> 20) * (1024 * 1024);
			if (blockoffset + m_meta.blockSize > m_mm.size())
				throw std::runtime_error("block offset out of bounds");

			return std::span<const char>{
				m_mm.data() + blockoffset,
					m_meta.blockSize
			};
		}

		std::span<const char> sector(size_t _lba) const
		{
			size_t blockIndex = _lba / m_meta.sectorsPerBlock;
			size_t blockOffset = _lba % m_meta.sectorsPerBlock;

			std::span<const char> b = block(blockIndex);
			return b.subspan(blockOffset * m_meta.logicalSectorSize, m_meta.logicalSectorSize);
		}

		const MetaInfo& info() const { return m_meta; }

	private:
		std::span<const uint64_t> m_BAT;
		MetaInfo m_meta;
		boost::iostreams::mapped_file_source m_mm;
	};
}

