/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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

/*
 * Do not include the regular gatery headers since this is meant to compile stand-alone in driver/userspace application code. 
 */

#include <filesystem>
#include <optional>


/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver::lnx {

class PCIDeviceFunctionList;

class PCIDeviceFunction
{
	public:
		PCIDeviceFunction(std::filesystem::path sysfsPath);

		std::uint16_t linuxDomain();
		std::uint8_t linuxBus();
		std::uint8_t linuxDevice();
		size_t function();

		void enable(bool enable = true);

		std::uint16_t vendorId() const;
		std::uint16_t deviceId() const;
		std::uint64_t deviceClass() const;
		size_t irq() const;

		std::filesystem::path resource(size_t bar = 0) const;
	protected:
		std::filesystem::path m_sysfsPath;

		mutable std::optional<std::uint16_t> m_linuxDomain;
		mutable std::optional<std::uint8_t> m_linuxBus;
		mutable std::optional<std::uint8_t> m_linuxDevice;
		mutable std::optional<size_t> m_function;

		mutable std::optional<std::uint16_t> m_vendorId;
		mutable std::optional<std::uint16_t> m_deviceId;
		mutable std::optional<std::uint64_t> m_deviceClass;
		mutable std::optional<size_t> m_irq;

		void parseSysfsPath() const;
		size_t readSingleHexFile(const std::filesystem::path &path) const;
		size_t readSingleDecFile(const std::filesystem::path &path) const;
};

class PCIDeviceFunctionList {
	public:
		PCIDeviceFunction findDeviceFunction(std::uint16_t domain, std::uint8_t bus, std::uint8_t device, size_t function) const;
		PCIDeviceFunction findDeviceFunction(std::uint16_t deviceId, std::uint16_t vendorId, size_t function) const;

		class iterator {
			public:
				iterator() { }

				bool operator==(const iterator &rhs) const;
				PCIDeviceFunction operator*();
				iterator &operator++();
			protected:
				std::filesystem::directory_iterator m_sysfsIterator;
				
				iterator(std::filesystem::path sysfsPath);
				friend class PCIDeviceFunctionList;
		};

		iterator begin() const;
		iterator end() const;
	protected:
		std::filesystem::path m_sysfsPath = "/sys/bus/pci/devices/";
};

}

/**@}*/