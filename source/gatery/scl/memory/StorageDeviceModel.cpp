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
#include "SdCardModel.h"
#include "StorageDeviceModel.h"

using namespace gtry::scl;

gtry::scl::StorageDeviceModelVHDX::StorageDeviceModelVHDX(std::unique_ptr<StorageDeviceModel> fallback) :
	StorageDeviceModel(move(fallback))
{
}

gtry::scl::StorageDeviceModelVHDX::StorageDeviceModelVHDX(std::unique_ptr<StorageDeviceModel> fallback, const std::filesystem::path& _vhdxFile) :
	StorageDeviceModelVHDX(move(fallback))
{
	open(_vhdxFile);
}

void StorageDeviceModelVHDX::open(const std::filesystem::path& _vhdxFile)
{
	m_vhdx.open(_vhdxFile);
}

uint64_t gtry::scl::StorageDeviceModelVHDX::numSectors() const
{
	return m_vhdx.info().diskSize / m_vhdx.info().logicalSectorSize;
}

StorageDeviceModelVHDX::Sector StorageDeviceModelVHDX::sectorRead(uint64_t sector)
{
	if (m_vhdx.sectorPresent(sector))
		return as_bytes(m_vhdx.sector(sector));
	return StorageDeviceModel::sectorRead(sector);
}

gtry::scl::StorageDeviceModelEmpty::StorageDeviceModelEmpty(std::unique_ptr<StorageDeviceModel> fallback, std::optional<uint64_t> numSectors, size_t sectorSize) :
	StorageDeviceModel(move(fallback)),
	m_emptySector(sectorSize)
{
	m_numSectors = numSectors;
}

StorageDeviceModel::Sector gtry::scl::StorageDeviceModelEmpty::sectorRead(uint64_t sector)
{
	return m_emptySector;
}

gtry::scl::StorageDeviceModel::StorageDeviceModel(std::unique_ptr<StorageDeviceModel> fallback) : 
	m_fallback(move(fallback))
{
}

uint64_t gtry::scl::StorageDeviceModel::numSectors() const
{
	if (m_numSectors)
		return *m_numSectors;
	return std::numeric_limits<uint64_t>::max();
}

StorageDeviceModel::Sector gtry::scl::StorageDeviceModel::sectorRead(uint64_t sector)
{
	if (m_fallback)
		return m_fallback->sectorRead(sector);
	throw std::runtime_error("not implemented");
}

void gtry::scl::StorageDeviceModel::sectorWrite(uint64_t sector, const Sector& data)
{
	if (m_fallback)
		m_fallback->sectorWrite(sector, data);
	throw std::runtime_error("not implemented");
}

void gtry::scl::StorageDeviceModel::sectorErase(uint64_t sector)
{
	if (m_fallback)
		m_fallback->sectorErase(sector);
	throw std::runtime_error("not implemented");
}

gtry::scl::StorageDeviceModelMemSparse::StorageDeviceModelMemSparse(std::unique_ptr<StorageDeviceModel> fallback, std::optional<uint64_t> numSectors, size_t sectorSize) :
	StorageDeviceModel(move(fallback)),
	m_erasedSector(sectorSize)
{
	m_numSectors = numSectors;
}

StorageDeviceModel::Sector gtry::scl::StorageDeviceModelMemSparse::sectorRead(uint64_t sector)
{
	auto it = m_storage.find(sector);
	if (it != m_storage.end())
	{
		if (it->second.empty())
			return m_erasedSector;
		return it->second;
	}
	return StorageDeviceModel::sectorRead(sector);
}

void gtry::scl::StorageDeviceModelMemSparse::sectorWrite(uint64_t sector, const Sector& data)
{
	std::vector<std::byte>& sectorData = m_storage[sector];
	sectorData.resize(data.size());
	std::copy(data.begin(), data.end(), sectorData.begin());
}

void gtry::scl::StorageDeviceModelMemSparse::sectorErase(uint64_t sector)
{
	std::vector<std::byte>& sectorData = m_storage[sector];
	sectorData.clear();
}
