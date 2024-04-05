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
		slave.address = mm.address.lower(slave.address.width());
		
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
				mm.writeData = slave.writeData->width();
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
		GroupScope entity(GroupScope::GroupType::ENTITY, "AvalonMMDemux");

		for (AvalonNetworkSection& sub : m_subSections)
			m_port.emplace_back(std::make_pair(sub.m_name, sub.demux()));

		BitWidth subAddressWidth = 0_b;
		for (const auto& port : m_port)
			subAddressWidth = std::max(subAddressWidth, port.second.address.width());

		const BitWidth portAddrWidth = BitWidth::count(m_port.size());
		AvalonMM ret;
		ret.address = portAddrWidth + subAddressWidth;

		for (size_t p = 0; p < m_port.size(); ++p)
		{
			AvalonMM& mm = m_port[p].second;
			Bit slaveSelect = ret.address(subAddressWidth.bits(), portAddrWidth) == p;
			attachSlave(ret, mm, slaveSelect);
		}
		return ret;
	}
	
	void AvalonMM::pinIn(std::string_view prefix)
	{
		std::string pinName = std::string{ prefix } + '_';

		// input pins
		address = gtry::pinIn(address.width()).setName(pinName + "address");
		if (read) *read = gtry::pinIn().setName(pinName + "read");
		if (write) *write = gtry::pinIn().setName(pinName + "write");
		if (writeData) *writeData = gtry::pinIn(writeData->width()).setName(pinName + "writedata");

		// output pins
		if (ready) gtry::pinOut(*ready).setName(pinName + "waitrequest_n");
		if (readData) gtry::pinOut(*readData).setName(pinName + "readdata");
		if (readDataValid) gtry::pinOut(*readDataValid).setName(pinName + "readdatavalid");
	}

	void AvalonMM::pinOut(std::string_view prefix)
	{
		std::string pinName = std::string{ prefix } + '_';

		// output pins
		gtry::pinOut(address).setName(pinName + "address");
		if (read) gtry::pinOut(*read).setName(pinName + "read");
		if (write) gtry::pinOut(*write).setName(pinName + "write");
		if (writeData) gtry::pinOut(*writeData).setName(pinName + "writedata");
		if (byteEnable) gtry::pinOut(*byteEnable).setName(pinName + "byteenable");

		// input pins
		if (ready) *ready = gtry::pinIn().setName(pinName + "waitrequest_n");
		if (readData) *readData = gtry::pinIn(readData->width()).setName(pinName + "readdata");
		if (readDataValid) *readDataValid = gtry::pinIn().setName(pinName + "readdatavalid");
	}

	void AvalonMM::setName(std::string_view prefix)
	{
		std::string name = std::string{ prefix };

		address.setName(name + "address");
		if (read) (*read).setName(name + "read");
		if (write) (*write).setName(name + "write");
		if (writeData) (*writeData).setName(name + "writedata");
		if (byteEnable) (*byteEnable).setName(name + "byteenable");

		// input pins
		if (ready) (*ready).setName(name + "waitrequest_n");
		if (readData) (*readData).setName(name + "readdata");
		if (readDataValid) (*readDataValid).setName(name + "readdatavalid");
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

	Memory<UInt> attachMem(AvalonMM& avmm, BitWidth addrWidth)
	{
		BitWidth dataWidth;
		if (avmm.readData)
			dataWidth = avmm.readData->width();
		else if (avmm.writeData)
			dataWidth = avmm.writeData->width();

		if (!addrWidth)
			addrWidth = avmm.address.width();

		if (avmm.ready)
			*avmm.ready = '1';

		Memory<UInt> mem{ (size_t) avmm.address.width().count(), dataWidth };

		if (avmm.readData)
		{
			avmm.readData = mem[avmm.address(0, addrWidth)];

			if (avmm.readDataValid)
				avmm.readDataValid = *avmm.read;

			for (size_t i = 0; i < avmm.readLatency; ++i)
			{
				avmm.readData = reg(*avmm.readData, {.allowRetimingBackward=true});

				if (avmm.readDataValid)
					avmm.readDataValid = reg(*avmm.readDataValid, '0');
			}
		}

		if (avmm.writeData)
		{
			IF(*avmm.write)
			{
				auto writePort = mem[avmm.address.lower(addrWidth)];
				if (avmm.byteEnable) {
					UInt currentMem = writePort.read();
					for (size_t i = 0; i < avmm.byteEnable->size(); i++)
					{
						IF ((*avmm.byteEnable)[i])
							currentMem.word(i, 8_b) = avmm.writeData->word(i, 8_b);
					}
					writePort = currentMem;
				}
				else
					writePort = *avmm.writeData;
			}
		}

		return mem;
	}

	template void AvalonMM::connect<UInt>(Memory<UInt>&, BitWidth);
}
