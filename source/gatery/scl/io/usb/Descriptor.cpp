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
#include "Descriptor.h"

#include <gatery/frontend.h>

gtry::scl::usb::EndpointAddress gtry::scl::usb::EndpointAddress::create(uint8_t index, gtry::scl::usb::EndpointDirection direction)
{
	return { uint8_t(index | (uint8_t(direction) << 7)) };
}

void gtry::scl::usb::Descriptor::add(StringId index, std::wstring_view string, LangID language)
{
	DescriptorEntry e;
	e.index = index.id;
	e.language = language;
	e.data.resize(string.size() * 2 + 2);

	HCL_ASSERT(e.data.size() < 256);
	e.data[0] = (uint8_t)e.data.size();
	e.data[1] = 3;
	for(size_t i = 0; i < string.size(); ++i)
	{
		e.data[2 + i * 2 + 0] = string[i] & 0xFF;
		e.data[2 + i * 2 + 1] = (string[i] >> 8) & 0xFF; 
	}

	auto it = std::find_if(m_entries.begin(), m_entries.end(), [](DescriptorEntry& e) {
		return e.type() == 3 && e.index == 0;
	});
	if(it == m_entries.end())
	{
		uint16_t langCode = uint16_t(language);

		DescriptorEntry langTable{
			.index = 0,
			.data = {4, 3, (uint8_t)langCode, (uint8_t)(langCode >> 8)}
		};
		m_entries.push_back(langTable);
	}
	else
	{
		uint16_t langCode = uint16_t(language);
		const uint16_t* codeList = (const uint16_t*)&it->data[2];
		for(size_t i = 0; i < it->data.size() / 2 - 1; ++i)
			if(codeList[i] == langCode)
				goto code_found;

		it->data.push_back((uint8_t)langCode);
		it->data.push_back((uint8_t)(langCode >> 8));
		HCL_ASSERT(it->data.size() < 256);
		it->data[0] = (uint8_t)it->data.size();
	code_found:;
	}
	m_entries.push_back(e);
}

void gtry::scl::usb::Descriptor::add(std::vector<uint8_t>&& data, uint8_t index)
{
	auto it = std::find_if(m_entries.begin(), m_entries.end(), [](DescriptorEntry& e) {
		return e.type() == 3 && e.index == 0;
	});

	m_entries.insert(it, DescriptorEntry{
		.index = index,
		.data = data
	});
}

gtry::scl::usb::StringId gtry::scl::usb::Descriptor::allocateStringIndex(std::wstring_view string, LangID language)
{
	StringId id = allocateStringIndex();
	add(id, string, language);
	return id;
}

void gtry::scl::usb::Descriptor::finalize()
{
	DescriptorEntry* desc[256] = {};
	std::bitset<256> epAddr;

	auto get = [&](auto t) -> decltype(t)&
	{
		using T = decltype(t);
		if(desc[T::TYPE])
			return desc[T::TYPE]->template decode<T>();

		static T dummy;
		return dummy;
	};

	uint8_t configIndex = 0;
	uint8_t interfaceIndex = 0;

	for(DescriptorEntry& e : m_entries)
	{
		desc[e.type()] = &e;
		if(e.type() != 3)
			get(ConfigurationDescriptor{}).TotalLength += (uint16_t)e.data.size();

		switch(e.type())
		{
		case ConfigurationDescriptor::TYPE:
			get(DeviceDescriptor{}).NumConfigurations++;

			{
				if(e.index == 0)
					e.index = configIndex++;

				ConfigurationDescriptor& config = get(ConfigurationDescriptor{});
				if(config.ConfigurationValue == 0)
					config.ConfigurationValue = e.index + 1;
			}
			break;

		case InterfaceDescriptor::TYPE:
			get(ConfigurationDescriptor{}).NumInterfaces++;

			{
				auto& iface = get(InterfaceDescriptor{});
				if(iface.InterfaceNumber == 0 && iface.AlternateSetting == 0)
					iface.InterfaceNumber = interfaceIndex++;

				auto& iad = get(InterfaceAssociationDescriptor{});
				if(iad.InterfaceCount == 0)
				{
					iad.FirstInterface = iface.InterfaceNumber;
					if(iad.FunctionClass == 0)
						iad.FunctionClass = iface.Class;
					if(iad.FunctionSubClass == 0)
						iad.FunctionSubClass = iface.SubClass;
				}
				iad.InterfaceCount++;
			}
			break;

		case EndpointDescriptor::TYPE:
			get(InterfaceDescriptor{}).NumEndpoints++;

			{
				uint8_t addr = get(EndpointDescriptor{}).Address.addr;
				HCL_DESIGNCHECK(!epAddr[addr]);
				epAddr[addr] = true;
			}
			break;

		case 3:
			// end config descriptor on first string
			desc[ConfigurationDescriptor::TYPE] = nullptr;
			break;

		default:
			break;
		}
	}

#if 0
	for(DescriptorEntry& e : m_descriptor)
	{
		for(uint8_t v : e.data)
			std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (size_t)v << ", ";
		std::cout << '\n';
	}
#endif
}

void gtry::scl::usb::Descriptor::changeMaxPacketSize(size_t value)
{
	HCL_DESIGNCHECK_HINT(utils::isPow2(value), "MaxPacketSize should be a power of two");
	HCL_DESIGNCHECK_HINT(value <= 64 && value >= 8, "MaxPacketSize should be between 8 and 64 byte");

	for (DescriptorEntry& e : m_entries)
	{
		if (e.type() == DeviceDescriptor::TYPE)
			e.decode<DeviceDescriptor>().MaxPacketSize = (uint8_t)value;
		else if (e.type() == EndpointDescriptor::TYPE)
			e.decode<EndpointDescriptor>().MaxPacketSize = (uint16_t)value;
	}
}

gtry::scl::usb::DeviceDescriptor* gtry::scl::usb::Descriptor::device()
{
	for (DescriptorEntry& e : m_entries)
		if (e.type() == DeviceDescriptor::TYPE)
			return &e.decode<DeviceDescriptor>();
	return nullptr;
}
