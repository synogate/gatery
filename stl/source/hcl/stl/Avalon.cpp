#include "Avalon.h"

namespace hcl::stl
{
	AvalonNetworkSection::AvalonNetworkSection(std::string name) :
		m_name(std::move(name))
	{}

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
	
	void AvalonMM::pinIn(std::string_view prefix)
	{
		std::string pinName = std::string{ prefix } + '_';

		// input pins
		address = hcl::pinIn(address.getWidth()).setName(pinName + "address");
		if (read) *read = hcl::pinIn().setName(pinName + "read");
		if (write) *write = hcl::pinIn().setName(pinName + "write");
		if (writeData) *writeData = hcl::pinIn(writeData->getWidth()).setName(pinName + "writedata");

		// output pins
		if (ready) hcl::pinOut(*ready).setName(pinName + "waitrequest_n");
		if (readData) hcl::pinOut(*readData).setName(pinName + "readdata");
		if (readDataValid) hcl::pinOut(*readDataValid).setName(pinName + "readdatavalid");
	}

	template void AvalonMM::connect<BVec>(Memory<BVec>&, BitWidth);
}
