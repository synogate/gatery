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
#include "Avalon.h"

namespace gtry::scl
{
	AvalonNetworkSection::AvalonNetworkSection(std::string name) :
		m_name(std::move(name))
	{}

	void AvalonNetworkSection::clear()
	{
		m_port.clear();
		m_subSections.clear();
	}

	void AvalonNetworkSection::add(std::string name, AvalonMM port)
	{
		std::string fullName = m_name;
		if (!fullName.empty())
			fullName += '_';
		fullName += name;

		auto& newPort = m_port.emplace_back(std::make_pair(std::move(name), std::move(port)));
		setName(newPort.second, fullName);
	}

	AvalonNetworkSection& AvalonNetworkSection::addSection(std::string name)
	{
		return m_subSections.emplace_back(std::move(name));
	}

	AvalonMM& AvalonNetworkSection::find(std::string_view path)
	{
		for (auto& sub : m_subSections)
		{
			const size_t nameLen = sub.m_name.size();
			if (path.size() > nameLen && path.substr(0, nameLen) == sub.m_name && path[nameLen] == '_')
				return sub.find(path.substr(nameLen + 1));
		}

		for (auto& port : m_port)
		{
			if (path == port.first)
				return port.second;
		}

		throw std::runtime_error("unable to find memory port " + std::string{ path });
	}
	
	void AvalonNetworkSection::assignPins()
	{
		for (auto& port : m_port)
		{
			std::string name = m_name;
			if (!name.empty())
				name += '_';
			port.second.pinIn(name + port.first);
		}
	}

	static void attachSlave(AvalonMM& mm, AvalonMM& slave, Bit slaveSelect)
	{
		slave.address = mm.address(0, slave.address.size());
		
		// write path
		if (slave.write)
		{
			if(!mm.write)
				mm.write = Bit{};
			slave.write = slaveSelect & *mm.write;
		}
		
		if (slave.writeData)
		{
			if (!mm.writeData)
				mm.writeData = slave.writeData->getWidth();
			slave.writeData = mm.writeData;
		}

		// read path
		if (slave.readData)
		{
			if (!mm.read)
			{
				mm.read = Bit{};
				mm.readData = *slave.readData;
				mm.readLatency = slave.readLatency;
			}
			else
			{
				if (mm.readDataValid.has_value() != slave.readDataValid.has_value())
				{
					mm.createReadDataValid();
					slave.createReadDataValid();

					*mm.readDataValid |= *slave.readDataValid;
					IF(*slave.readDataValid)
						*mm.readData = *slave.readData;
				}
				else
				{
					const size_t latency = std::max(mm.readLatency, slave.readLatency);
					mm.createReadLatency(latency);
					slave.createReadLatency(latency);

					Bit readSelectSlave = slaveSelect & *mm.read;
					HCL_NAMED(readSelectSlave);

					for (size_t i = 0; i < latency; ++i)
						readSelectSlave = reg(readSelectSlave, '0');

					IF(readSelectSlave)
						* mm.readData = *slave.readData;
				}
			}

			slave.read = slaveSelect & *mm.read;
		}
	}

	AvalonMM AvalonNetworkSection::demux()
	{
		GroupScope entity(GroupScope::GroupType::ENTITY);
		entity.setName("AvalonMMDemux");

		for (AvalonNetworkSection& sub : m_subSections)
			m_port.emplace_back(std::make_pair(sub.m_name, sub.demux()));

		size_t subAddressWidth = 0;
		for (const auto& port : m_port)
			subAddressWidth = std::max(subAddressWidth, port.second.address.size());

		const size_t portAddrWidth = utils::Log2C(m_port.size());
		AvalonMM ret;
		ret.address = BitWidth{ portAddrWidth + subAddressWidth };

		for (size_t p = 0; p < m_port.size(); ++p)
		{
			AvalonMM& mm = m_port[p].second;
			Bit slaveSelect = ret.address(subAddressWidth, portAddrWidth) == p;
			attachSlave(ret, mm, slaveSelect);
		}
		return ret;
	}
	
	void AvalonMM::pinIn(std::string_view prefix)
	{
		std::string pinName = std::string{ prefix } + '_';

		// input pins
		address = gtry::pinIn(address.getWidth()).setName(pinName + "address");
		if (read) *read = gtry::pinIn().setName(pinName + "read");
		if (write) *write = gtry::pinIn().setName(pinName + "write");
		if (writeData) *writeData = gtry::pinIn(writeData->getWidth()).setName(pinName + "writedata");

		// output pins
		if (ready) gtry::pinOut(*ready).setName(pinName + "waitrequest_n");
		if (readData) gtry::pinOut(*readData).setName(pinName + "readdata");
		if (readDataValid) gtry::pinOut(*readDataValid).setName(pinName + "readdatavalid");
	}

	void AvalonMM::createReadDataValid()
	{
		if (readDataValid)
			return;
		readDataValid = read;
		for (size_t i = 0; i < readLatency; ++i)
			readDataValid = reg(*readDataValid, '0');
	}

	void AvalonMM::createReadLatency(size_t targetLatency)
	{
		HCL_DESIGNCHECK_HINT(!readDataValid, "interfaces with read data valid signal are dynamic latecy interfaces");
		HCL_DESIGNCHECK(targetLatency >= readLatency);

		if (readData)
		{
			for (size_t i = readLatency; i < targetLatency; ++i)
				*readData = reg(*readData);
		}
		readLatency = targetLatency;
	}

	template void AvalonMM::connect<BVec>(Memory<BVec>&, BitWidth);
}
