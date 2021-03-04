#include "AvalonMM.h"

namespace hcl::stl
{
	AvalonMMSlave::AvalonMMSlave(BitWidth addrWidth, BitWidth dataWidth) :
		address(addrWidth),
		writeData(dataWidth),
		readData(dataWidth)
	{
		write.setResetValue('0');
		readData = 0;
	}

	void AvalonMMSlave::ro(const BVec& value, RegDesc desc)
	{
		for (size_t offset = 0; offset < value.size(); offset += readData.size())
		{
			const size_t width = std::min(readData.size(), value.size() - offset);

			IF(address == addressMap.size())
				readData = zext(value(offset, width));

			RegDesc d = desc;
			if (readData.size() < value.size())
				d.name += std::to_string(offset / readData.size());
			addressMap.push_back(d);
		}
	}

	void AvalonMMSlave::ro(const Bit& value, RegDesc desc)
	{
		IF(address == addressMap.size())
			readData = zext(value);
		addressMap.push_back(desc);
	}

	Bit AvalonMMSlave::rw(BVec& value, RegDesc desc)
	{
		Bit selected = '0';
		for (size_t offset = 0; offset < value.size(); offset += readData.size())
		{
			const size_t width = std::min(readData.size(), value.size() - offset);

			IF(address == addressMap.size())
			{
				readData = zext(value(offset, width));
				IF(write)
				{
					selected = '1';
					value(offset, width) = writeData(0, width);
				}
			}

			RegDesc d = desc;
			if (readData.size() < value.size())
				d.name += std::to_string(offset / readData.size());
			addressMap.push_back(d);
		}
		setName(value, desc.name);
		return selected;
	}

	Bit AvalonMMSlave::rw(Bit& value, RegDesc desc)
	{
		Bit selected = '0';

		IF(address == addressMap.size())
		{
			readData = zext(value);
			IF(write)
			{
				selected = '1';
				value = writeData[0];
			}
		}
		setName(selected, desc.name + "_selected");
		setName(value, desc.name);

		addressMap.push_back(desc);
		return selected;
	}

	void pinIn(AvalonMMSlave& avmm, std::string prefix)
	{
		avmm.address = hcl::pinIn(avmm.address.getWidth()).setName(prefix + "_address");
		avmm.write = hcl::pinIn().setName(prefix + "_write");
		avmm.writeData = hcl::pinIn(avmm.writeData.getWidth()).setName(prefix + "_write_data");
		hcl::pinOut(avmm.readData).setName(prefix + "_read_data");
	}
}
