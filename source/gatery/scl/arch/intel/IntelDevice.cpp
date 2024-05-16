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

#include "IntelDevice.h"

#include "../general/GenericMemory.h"

#include "eSRAM.h"
#include "M20K.h"
#include "M9K.h"
#include "MLAB.h"
#include "GLOBAL.h"
#include "TRI.h"
#include "ALTDDIO_OUT.h"


#include <regex>



namespace gtry::scl::arch::intel {



struct AgilexDeviceString {
	char series;
	size_t logicElementsDigits;
	size_t logicElements;
	size_t transcieverSpeedGrade;
	char power;
	size_t fabricSpeedGrade;

	bool parse(const std::string &device) {
		static auto deviceRegex = std::regex("AG(F|I|M)(A|B|C|D)(\\d\\d\\d)(R\\d\\d.)(\\d)(E|I)(\\d)(V|E|F|X).*");

		std::smatch matchResult;

		if (!std::regex_match(device, matchResult, deviceRegex)) return false;
		
		series = matchResult[1].str()[0];
		logicElementsDigits = boost::lexical_cast<size_t>(matchResult[3].str());
		logicElements = logicElementsDigits * 100; // Actually not entirely correct
		transcieverSpeedGrade = boost::lexical_cast<size_t>(matchResult[5].str());
		fabricSpeedGrade = boost::lexical_cast<size_t>(matchResult[7].str());
		power = matchResult[8].str()[0];

		return true;
	}
};


struct Arria10DeviceString {
	enum {
		GX,
		GT
	} variant;
	size_t logicElements;
	size_t transcieverCount;
	size_t transcieverSpeedGrade;
	size_t fabricSpeedGrade;

	bool parse(const std::string &device) {
		static auto deviceRegex = std::regex("10A(X|T)(\\d\\d\\d)(C|E|H|K|N|R|S|U)(\\d)(F|U)(\\d\\d)(I|E|M)(\\d)(H|S|L|V)(G|N|P)(ES)?");

		std::smatch matchResult;

		if (!std::regex_match(device, matchResult, deviceRegex)) return false;
		
		variant = (matchResult[1].str() == "X")?GX:GT;
		logicElements = boost::lexical_cast<size_t>(matchResult[2].str()) * 10'000;
		char transcieverCountChar = matchResult[3].str()[0];
		switch (transcieverCountChar) {
			case 'C': transcieverCount = 6; break;
			case 'E': transcieverCount = 12; break;
			case 'H': transcieverCount = 24; break;
			case 'K': transcieverCount = 36; break;
			case 'N': transcieverCount = 48; break;
			case 'R': transcieverCount = 66; break;
			case 'S': transcieverCount = 72; break;
			case 'U': transcieverCount = 96; break;
		}
		transcieverSpeedGrade = boost::lexical_cast<size_t>(matchResult[4].str());
		fabricSpeedGrade = boost::lexical_cast<size_t>(matchResult[8].str());

		return true;
	}
};




struct Stratix10DeviceString {
	char variant;
	char power;
	std::string logicElementsDigits;
	size_t logicElements;
	size_t transcieverCount;
	size_t transcieverSpeedGrade;
	size_t fabricSpeedGrade;

	bool parse(const std::string &device) {
		static auto deviceRegex = std::regex("1S(G|X)(\\d\\d[M\\d])(L|H)(H|N|U)(\\d)F(\\d\\d)(I|E|C)(\\d)(V|L|X)(G|P).*");

		std::smatch matchResult;

		if (!std::regex_match(device, matchResult, deviceRegex)) return false;
		
		variant = matchResult[1].str()[0];
		logicElementsDigits = matchResult[2];
		if (logicElementsDigits == "10M")
			logicElements = 10'200'000;
		else
			logicElements = boost::lexical_cast<size_t>(logicElementsDigits) * 10'000;

		char transcieverCountChar = matchResult[4].str()[0];
		switch (transcieverCountChar) {
			case 'H': transcieverCount = 24; break;
			case 'N': transcieverCount = 48; break;
			case 'U': transcieverCount = 96; break;
		}
		transcieverSpeedGrade = boost::lexical_cast<size_t>(matchResult[5].str());
		fabricSpeedGrade = boost::lexical_cast<size_t>(matchResult[9].str());
		power = matchResult[10].str()[0];

		return true;
	}
};


struct Cyclone10DeviceString {
	enum {
		GX,
		LP
	} variant;
	size_t logicElements;
	size_t fabricSpeedGrade;

	bool parse(const std::string &device) {
		static auto deviceRegex = std::regex("10C(X|L)(\\d\\d\\d)(Y|Z)(F|E|U|M)(\\d\\d\\d)(I|C|A)(\\d)(G)?(ES)?");

		std::smatch matchResult;

		if (!std::regex_match(device, matchResult, deviceRegex)) return false;
		
		variant = (matchResult[1].str() == "X")?GX:LP;
		logicElements = boost::lexical_cast<size_t>(matchResult[2].str()) * 1'000;
		fabricSpeedGrade = boost::lexical_cast<size_t>(matchResult[7].str());

		return true;
	}
};


struct MAX10DeviceString {
	size_t logicElements;
	size_t fabricSpeedGrade;

	bool parse(const std::string &device) {
		static auto deviceRegex = std::regex("10M(\\d\\d)(SC|SA|DC|DF|DA)(V|E|M|U|F)(\\d\\d\\d?)(I|C|A)(\\d)G?(ES)?P?");

		std::smatch matchResult;

		if (!std::regex_match(device, matchResult, deviceRegex)) return false;
		
		logicElements = boost::lexical_cast<size_t>(matchResult[1].str()) * 1'000;
		fabricSpeedGrade = boost::lexical_cast<size_t>(matchResult[6].str());

		return true;
	}
};





void IntelDevice::fromConfig(const gtry::utils::ConfigTree &configTree)
{
	FPGADevice::fromConfig(configTree);

	auto customComposition = configTree["custom_composition"];
	if (customComposition) {
		setupCustomComposition(customComposition);
	} else {
		if (!m_device.empty())
			setupDevice(std::move(m_device));
		else if (!m_family.empty()) {
			if (m_family == "Cyclone 10")
				setupCyclone10();
			else if (m_family == "Arria 10")
				setupArria10();
			else if (m_family == "Stratix 10")
				setupStratix10();
			else if (m_family == "MAX 10")
				setupMAX10();
			else if (m_family == "Agilex")
				setupAgilex();
			else
				HCL_DESIGNCHECK_HINT(false, "The device family " + m_family + " is not among the supported device families. Use custom_composition to specify the device's hardware features.");
		} else {
			// Default to a Cyclone 10, since those are big but can still be synthesized with the license free quartus pro
			setupCyclone10();
		}
	}
}

std::string IntelDevice::nextLpmInstanceName(std::string_view macroType)
{
	return std::string{ "gatery_" } + std::string{ macroType } + '_' + std::to_string(m_lpmInstanceCounter[macroType]++);
}

void IntelDevice::setupAgilex()
{
	setupDevice("AGFA012R24B1E1V");
}

void IntelDevice::setupArria10()
{
	setupDevice("10AX115U1F45I1SG");
}

void IntelDevice::setupStratix10()
{
	setupDevice("1SG10MLN1F74I1VG");
}

void IntelDevice::setupCyclone10()
{
	setupDevice("10CX220YF780I5G");
}

void IntelDevice::setupMAX10()
{
	//setupDevice("10M50DAF672I6");
	setupDevice("10M08DAF484C8G");
}


void IntelDevice::setupCustomComposition(const gtry::utils::ConfigTree &customComposition)
{
	m_embeddedMemoryList = std::make_unique<EmbeddedMemoryList>();

	if (customComposition["MLAB"].as(false))
		m_embeddedMemoryList->add(std::make_unique<MLAB>(*this));

	if (customComposition["M9K"].as(false))
		m_embeddedMemoryList->add(std::make_unique<M9K>(*this));
	if (customComposition["M20K"].as(false))
		m_embeddedMemoryList->add(std::make_unique<M20K>(*this));
	if (customComposition["M20KStratix10Agilex"].as(false))
		m_embeddedMemoryList->add(std::make_unique<M20KStratix10Agilex>(*this));
	if (customComposition["eSRAM"].as(false))
		m_embeddedMemoryList->add(std::make_unique<eSRAM>(*this));

	m_technologyMapping.addPattern(std::make_unique<EmbeddedMemoryPattern>(*this));

	if (customComposition["GLOBAL"].as(false))
		m_technologyMapping.addPattern(std::make_unique<GLOBALPattern>());
	
	if (customComposition["ALTDDIO_OUT"].as(false))
		m_technologyMapping.addPattern(std::make_unique<ALTDDIO_OUTPattern>(*this));
}


void IntelDevice::setupDevice(std::string device)
{
	m_vendor = "intel";
	m_device = std::move(device);

	m_embeddedMemoryList = std::make_unique<EmbeddedMemoryList>();
	m_technologyMapping.addPattern(std::make_unique<EmbeddedMemoryPattern>(*this));

	AgilexDeviceString agilexDevStr;
	Arria10DeviceString arria10DevStr;
	Stratix10DeviceString stratix10DevStr;
	Cyclone10DeviceString cyclone10DevStr;
	MAX10DeviceString max10DevStr;

	if (agilexDevStr.parse(m_device)) {
		m_family = "Agilex";
		bool add_eSRAM = false;
		bool add_crypto = false;

		// Intel Agilex FPGAs and SoCs Device Overview "Table 3. Intel Agilex F-Series FPGAs and SoCs Family Plan Part-1"
		if (agilexDevStr.series == 'F' && (agilexDevStr.logicElementsDigits >= 12 && agilexDevStr.logicElementsDigits <= 23))
			add_eSRAM = true;

		// Intel Agilex FPGAs and SoCs Device Overview "Table 3. Intel Agilex F-Series FPGAs and SoCs Family Plan Part-1"
		if (agilexDevStr.series == 'F' && (agilexDevStr.logicElementsDigits >= 19 && agilexDevStr.logicElementsDigits <= 23))
			add_crypto = true;

		// Intel Agilex FPGAs and SoCs Device Overview "Table 7. Intel Agilex I-Series SoC FPGAs Family Plan Part-1"
		if (agilexDevStr.series == 'I' && (agilexDevStr.logicElementsDigits >= 19 && agilexDevStr.logicElementsDigits <= 23)) {
			add_eSRAM = true;
			add_crypto  = true;
		}

		{
			// Intel Agilex Device Data Sheet "Table 32. Memory Block Performance Specifications for Intel Agilex Devices"
			[[maybe_unused]] const size_t MHz_dont_care[] = {1000, 782, 667, 600};
			[[maybe_unused]] const size_t MHz_rdw[] = {630, 510, 460, 320};
			m_embeddedMemoryList->add(std::make_unique<MLAB>(*this));
		}
		{
			// Intel Agilex Device Data Sheet "Table 32. Memory Block Performance Specifications for Intel Agilex Devices"
			[[maybe_unused]] const size_t MHz_max[] = {1000, 782, 667, 600};
			[[maybe_unused]] const size_t MHz_min_noecc[] = {600, 500, 420, 360};
			m_embeddedMemoryList->add(std::make_unique<M20KStratix10Agilex>(*this));
		}

		if (add_eSRAM) {
			// Intel Agilex Device Data Sheet "Table 32. Memory Block Performance Specifications for Intel Agilex Devices"
			[[maybe_unused]] const size_t MHz_max[] = {750, 640, 500, 500};
			//m_embeddedMemoryList->add(std::make_unique<eSRAM>(*this));
		}

		if (add_crypto) {
		}

		m_technologyMapping.addPattern(std::make_unique<GLOBALPattern>());
		m_technologyMapping.addPattern(std::make_unique<TRIPattern>());
		m_technologyMapping.addPattern(std::make_unique<ALTDDIO_OUTPattern>(*this));

	} else if (arria10DevStr.parse(m_device)) {
		m_family = "Arria 10";
		m_requiresDerivePllClocks = true;

		m_embeddedMemoryList->add(std::make_unique<MLAB>(*this));
		m_embeddedMemoryList->add(std::make_unique<M20K>(*this));
		m_technologyMapping.addPattern(std::make_unique<GLOBALPattern>());
		m_technologyMapping.addPattern(std::make_unique<TRIPattern>());
		m_technologyMapping.addPattern(std::make_unique<ALTDDIO_OUTPattern>(*this));

	} else if (stratix10DevStr.parse(m_device)) {
		m_family = "Stratix 10";

		m_embeddedMemoryList->add(std::make_unique<MLAB>(*this));
		m_embeddedMemoryList->add(std::make_unique<M20KStratix10Agilex>(*this));
		m_technologyMapping.addPattern(std::make_unique<GLOBALPattern>());
		m_technologyMapping.addPattern(std::make_unique<TRIPattern>());
		m_technologyMapping.addPattern(std::make_unique<ALTDDIO_OUTPattern>(*this));

	} else if (cyclone10DevStr.parse(m_device)) {

		if (cyclone10DevStr.variant == Cyclone10DeviceString::GX) {
			m_family = "Cyclone 10 GX";
			m_requiresDerivePllClocks = true;
			m_embeddedMemoryList->add(std::make_unique<MLAB>(*this));
			m_embeddedMemoryList->add(std::make_unique<M20K>(*this));
		} else {
			m_family = "Cyclone 10 LP";
			m_embeddedMemoryList->add(std::make_unique<M9K>(*this));
		}
		m_technologyMapping.addPattern(std::make_unique<GLOBALPattern>());
		m_technologyMapping.addPattern(std::make_unique<TRIPattern>());
		m_technologyMapping.addPattern(std::make_unique<ALTDDIO_OUTPattern>(*this));

	} else if (max10DevStr.parse(m_device)) {
		m_family = "MAX 10";

		m_embeddedMemoryList->add(std::make_unique<M9K>(*this));
		m_technologyMapping.addPattern(std::make_unique<GLOBALPattern>());
		m_technologyMapping.addPattern(std::make_unique<TRIPattern>());
		m_technologyMapping.addPattern(std::make_unique<ALTDDIO_OUTPattern>(*this));
	   
	} else
		HCL_DESIGNCHECK_HINT(false, "The device string " + m_device + " does not match the pattern of any of the known device families. Specify a familiy or use custom_composition to specify the device's hardware features.");
}



}
