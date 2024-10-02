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

#include "XilinxDevice.h"

#include "../general/GenericMemory.h"

#include "OBUFDS.h"
#include "BUFG.h"
#include "ODDR.h"
#include "FifoPattern.h"
#include "BlockramUltrascale.h"
#include "Lutram7Series.h"
#include "LutramUltrascale.h"
#include "DSP48E2.h"

#include <regex>



namespace gtry::scl::arch::xilinx {



struct Zynq7DeviceString {
	bool lowPower;
	size_t fabricSpeedGrade;
	size_t valueIndex;
	size_t logicCells;
	
	bool parse(const std::string &device) {
		static auto deviceRegex = std::regex("XC7Z(\\d\\d\\d)S?-(L?)(\\d)(CL|SB|FB|FF)(V|G)(\\d\\d\\d)(C|E|I)");

		std::smatch matchResult;

		if (!std::regex_match(device, matchResult, deviceRegex)) return false;
		lowPower = matchResult[2].str() == "L";
		fabricSpeedGrade = boost::lexical_cast<size_t>(matchResult[3].str());

		valueIndex = boost::lexical_cast<size_t>(matchResult[1].str());
		switch (valueIndex) {
			case 7: 
				logicCells = 23'000;
			break;
			case 12: 
				logicCells = 55'000;
			break;
			case 14: 
				logicCells = 65'000;
			break;
			case 10: 
				logicCells = 28'000;
			break;
			case 15: 
				logicCells = 74'000;
			break;
			case 20: 
				logicCells = 85'000;
			break;
			case 30: 
				logicCells = 125'000;
			break;
			case 35: 
				logicCells = 275'000;
			break;
			case 45: 
				logicCells = 350'000;
			break;
			case 100: 
				logicCells = 444'000;
			break;
			default:
				logicCells = 0;
			break;
		}

		return true;
	}
};


struct KintexVirtexUltrascaleDeviceString {
	char KintexVirtex;
	bool lowPower;
	size_t fabricSpeedGrade;
	size_t valueIndex;
	//size_t logicCells;
	
	bool parse(const std::string &device) {
		static auto deviceRegex = std::regex("XC(K|V)U(\\d\\d\\d)-(L|H)?(\\d)(F|S)(F|L|B)(V|G)A(\\d?\\d\\d\\d)(C|E|I)");

		std::smatch matchResult;

		if (!std::regex_match(device, matchResult, deviceRegex)) return false;
		KintexVirtex = matchResult[1].str()[0];
		valueIndex = boost::lexical_cast<size_t>(matchResult[2].str());
	
		lowPower = matchResult[3].str() == "L";
		fabricSpeedGrade = boost::lexical_cast<size_t>(matchResult[4].str());
		
		return true;
	}
};



void XilinxDevice::fromConfig(const gtry::utils::ConfigTree &configTree)
{
	FPGADevice::fromConfig(configTree);
	
	auto customComposition = configTree["custom_composition"];
	if (customComposition) {
		setupCustomComposition(customComposition);
	} else {
		if (!m_device.empty())
			setupDevice(std::move(m_device));
		else if (!m_family.empty()) {
			if (m_family == "Zynq7")
				setupZynq7();
			else if (m_family == "Kintex Ultrascale")
				setupKintexUltrascale();
			else if (m_family == "Virtex Ultrascale")
				setupVirtexUltrascale();
			else
				HCL_DESIGNCHECK_HINT(false, "The device family " + m_family + " is not among the supported device families. Use custom_composition to specify the device's hardware features.");
		} else {
			setupZynq7();
		}
	}
}

void XilinxDevice::setupZynq7()
{
	setupDevice("XC7Z100-3FFG900I");
}

void XilinxDevice::setupKintexUltrascale()
{
	setupDevice("XCKU035-1FBVA900C");
}

void XilinxDevice::setupVirtexUltrascale()
{
	setupDevice("XCVU190-1FBVA900C");
}

void XilinxDevice::setupCustomComposition(const gtry::utils::ConfigTree &customComposition)
{
	m_embeddedMemoryList = std::make_unique<EmbeddedMemoryList>();

	if (customComposition["Lutram7Series"].as(false))
		m_embeddedMemoryList->add(std::make_unique<Lutram7Series>(*this));

	if (customComposition["LutramUltrascale"].as(false))
		m_embeddedMemoryList->add(std::make_unique<LutramUltrascale>(*this));

	if (customComposition["BlockramUltrascale"].as(false))
		m_embeddedMemoryList->add(std::make_unique<BlockramUltrascale>(*this));

	m_technologyMapping.addPattern(std::make_unique<EmbeddedMemoryPattern>(*this));

	if (customComposition["DSP48E2"].as(false))
		m_technologyMapping.addPattern(std::make_unique<PipelinedMulDSP48E2Pattern>());

	if (customComposition["BUFG"].as(false))
		m_technologyMapping.addPattern(std::make_unique<BUFGPattern>());

	if (customComposition["ODDR"].as(false))
		m_technologyMapping.addPattern(std::make_unique<ODDRPattern>());
}


void XilinxDevice::setupDevice(std::string device)
{
	m_vendor = "xilinx";
	m_device = std::move(device);

	m_embeddedMemoryList = std::make_unique<EmbeddedMemoryList>();
	m_technologyMapping.addPattern(std::make_unique<EmbeddedMemoryPattern>(*this));

	Zynq7DeviceString zynq7DevStr;
	KintexVirtexUltrascaleDeviceString kintexVirtexUltraDevStr;

	if (zynq7DevStr.parse(m_device)) {
		m_family = "Zynq7";

		m_embeddedMemoryList->add(std::make_unique<Lutram7Series>(*this));

		m_technologyMapping.addPattern(std::make_unique<BUFGPattern>());
		m_technologyMapping.addPattern(std::make_unique<ODDRPattern>());

		// Has DSP48E1
		
	} else if (kintexVirtexUltraDevStr.parse(m_device)) {
		if (kintexVirtexUltraDevStr.KintexVirtex == 'K')
			m_family = "Kintex Ultrascale";
		else
			m_family = "Virtex Ultrascale";

		m_embeddedMemoryList->add(std::make_unique<LutramUltrascale>(*this));
		m_embeddedMemoryList->add(std::make_unique<BlockramUltrascale>(*this));

		m_technologyMapping.addPattern(std::make_unique<BUFGPattern>());
		m_technologyMapping.addPattern(std::make_unique<ODDRPattern>());
		m_technologyMapping.addPattern(std::make_unique<PipelinedMulDSP48E2Pattern>());

	} else
		HCL_DESIGNCHECK_HINT(false, "The device string " + m_device + " does not match the pattern of any of the known device families. Specify a familiy or use custom_composition to specify the device's hardware features.");
}



}