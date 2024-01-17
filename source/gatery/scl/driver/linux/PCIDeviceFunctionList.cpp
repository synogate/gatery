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

/*
 * Do not include the regular gatery headers since this is meant to compile stand-alone in driver/userspace application code. 
 */
#include "PCIDeviceFunctionList.h"

#include <boost/format.hpp>
#include <boost/regex.hpp>

#include <fstream>

namespace gtry::scl::driver::lnx {

static const boost::regex functionFilenameRegex(".*([0-9a-fA-F]{4}):([0-9a-fA-F]{2}):([0-9a-fA-F]{2})\\.([0-9a-fA-F])/?");


PCIDeviceFunction::PCIDeviceFunction(std::filesystem::path sysfsPath) : m_sysfsPath(std::move(sysfsPath))
{
	if (!std::filesystem::exists(m_sysfsPath))
		throw std::runtime_error("PCIe device does not exist in sysfs path!");
}

std::uint16_t PCIDeviceFunction::linuxDomain()
{
	if (!m_linuxDomain)
		parseSysfsPath();
	return *m_linuxDomain;
}

std::uint8_t PCIDeviceFunction::linuxBus()
{
	if (!m_linuxBus)
		parseSysfsPath();
	return *m_linuxBus;
}

std::uint8_t PCIDeviceFunction::linuxDevice()
{
	if (!m_linuxDevice)
		parseSysfsPath();
	return *m_linuxDevice;
}

size_t PCIDeviceFunction::function()
{
	if (!m_function)
		parseSysfsPath();
	return *m_function;
}

void PCIDeviceFunction::enable(bool enable)
{
	std::ofstream{m_sysfsPath / "enable"} << (enable?'1':'0');
}

std::uint16_t PCIDeviceFunction::vendorId() const
{
	if (!m_vendorId)
		m_vendorId = readSingleHexFile(m_sysfsPath / "vendor");
	return *m_vendorId;
}

std::uint16_t PCIDeviceFunction::deviceId() const
{
	if (!m_deviceId)
		m_deviceId = readSingleHexFile(m_sysfsPath / "device");
	return *m_deviceId;
}

std::uint64_t PCIDeviceFunction::deviceClass() const
{
	if (!m_deviceClass)
		m_deviceClass = readSingleHexFile(m_sysfsPath / "class");
	return *m_deviceClass;
}

size_t PCIDeviceFunction::irq() const
{
	if (!m_irq)
		m_irq = readSingleDecFile(m_sysfsPath / "irq");
	return *m_irq;
}


std::filesystem::path PCIDeviceFunction::resource(size_t bar) const
{
	return m_sysfsPath / (boost::format("resource%d") % bar).str();
}


void PCIDeviceFunction::parseSysfsPath() const
{
	boost::smatch matches;
	if (!boost::regex_match(m_sysfsPath.string(), matches, functionFilenameRegex))
		throw std::runtime_error("Invalid sysfs path");

	m_linuxDomain = std::stoul(matches[0], nullptr, 16);
	m_linuxBus = std::stoul(matches[1], nullptr, 16);
	m_linuxDevice = std::stoul(matches[2], nullptr, 16);
	m_function = std::stoul(matches[3], nullptr, 16);
}

size_t PCIDeviceFunction::readSingleHexFile(const std::filesystem::path &path) const
{
	size_t result;
	std::ifstream file(path.string().c_str());
	// skip leading "0x"
	file.get();
	file.get();
	file >> std::hex >> result;
	return result;
}

size_t PCIDeviceFunction::readSingleDecFile(const std::filesystem::path &path) const
{
	size_t result;
	std::ifstream file(path.string().c_str());
	file >> result;
	return result;
}



PCIDeviceFunctionList::iterator::iterator(std::filesystem::path sysfsPath)
{
	m_sysfsIterator = std::filesystem::directory_iterator(sysfsPath);
	while (m_sysfsIterator != std::filesystem::directory_iterator()) {
		if (boost::regex_match(m_sysfsIterator->path().string(), functionFilenameRegex))
			break;
		else
			++m_sysfsIterator;
	}
}


bool PCIDeviceFunctionList::iterator::operator==(const iterator &rhs) const
{
	return m_sysfsIterator == rhs.m_sysfsIterator;
}

PCIDeviceFunction PCIDeviceFunctionList::iterator::operator*()
{
	return PCIDeviceFunction(m_sysfsIterator->path());
}

PCIDeviceFunctionList::iterator &PCIDeviceFunctionList::iterator::operator++()
{
	do {
		++m_sysfsIterator;
	} while (m_sysfsIterator != std::filesystem::directory_iterator() && !boost::regex_match(m_sysfsIterator->path().string(), functionFilenameRegex));

	return *this;
}




PCIDeviceFunction PCIDeviceFunctionList::findDeviceFunction(std::uint16_t domain, std::uint8_t bus, std::uint8_t device, size_t function) const
{
	return PCIDeviceFunction(m_sysfsPath / (boost::format("%04x:%02x:%02x.%01x") % (unsigned) domain % (unsigned) bus % (unsigned) device % (unsigned) function).str());
}

PCIDeviceFunction PCIDeviceFunctionList::findDeviceFunction(std::uint16_t deviceId, std::uint16_t vendorId, size_t function) const
{
	for (auto endpoint : *this)
		if (endpoint.deviceId() == deviceId && endpoint.vendorId() == vendorId && endpoint.function() == function)
			return endpoint;

	throw std::runtime_error("No device endpoint that matches the given device-id, vendor-id and function could be found in the sysfs path");
}

PCIDeviceFunctionList::iterator PCIDeviceFunctionList::begin() const
{
	return iterator(m_sysfsPath);
}

PCIDeviceFunctionList::iterator PCIDeviceFunctionList::end() const
{
	return iterator();
}

}