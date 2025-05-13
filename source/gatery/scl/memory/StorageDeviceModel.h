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
#include <gatery/frontend.h>
#include "../driver/VHDXReader.h"

namespace gtry::scl
{
	class StorageDeviceModel
	{
	public:
		using Sector = std::span<const std::byte>;

	public:
		StorageDeviceModel(std::unique_ptr<StorageDeviceModel> fallback);

		virtual ~StorageDeviceModel() = default;

		virtual uint64_t numSectors() const;

		virtual Sector sectorRead(uint64_t sector);
		virtual void sectorWrite(uint64_t sector, const Sector& data);
		virtual void sectorErase(uint64_t sector);

	protected:
		std::unique_ptr<StorageDeviceModel> m_fallback;
		std::optional<uint64_t> m_numSectors;
	};

	class StorageDeviceModelEmpty : public StorageDeviceModel
	{
	public:
		StorageDeviceModelEmpty(std::unique_ptr<StorageDeviceModel> fallback = {}, std::optional<uint64_t> numSectors = {}, size_t sectorSize = 512);

		virtual Sector sectorRead(uint64_t sector) override;
	private:
		std::vector<std::byte> m_emptySector;
	};

	class StorageDeviceModelVHDX : public StorageDeviceModel
	{
	public:
		StorageDeviceModelVHDX(std::unique_ptr<StorageDeviceModel> fallback);
		StorageDeviceModelVHDX(std::unique_ptr<StorageDeviceModel> fallback, const std::filesystem::path& _vhdxFile);

		void open(const std::filesystem::path& _vhdxFile);

		virtual uint64_t numSectors() const override;
		virtual Sector sectorRead(uint64_t sector) override;

	protected:
		VHDXReader m_vhdx;
	};

	class StorageDeviceModelMemSparse : public StorageDeviceModel
	{
	public:
		StorageDeviceModelMemSparse(std::unique_ptr<StorageDeviceModel> fallback, std::optional<uint64_t> numSectors = {}, size_t sectorSize = 512);

		virtual Sector sectorRead(uint64_t sector) override;
		virtual void sectorWrite(uint64_t sector, const Sector& data) override;
		virtual void sectorErase(uint64_t sector) override;

	protected:
		std::map<uint64_t, std::vector<std::byte>> m_storage;
		std::vector<std::byte> m_erasedSector;
	};

}
