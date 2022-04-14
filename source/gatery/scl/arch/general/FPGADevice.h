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

#include "GenericMemory.h"
#include <gatery/frontend/tech/TargetTechnology.h>

#include <string>
#include <memory>
#include <vector>

namespace gtry::scl::arch {

class GenericMemoryCapabilities;

class FPGADevice : public TargetTechnology {
	public:
		FPGADevice();
		inline const EmbeddedMemoryList &getEmbeddedMemories() const { return *m_embeddedMemoryList; }

		inline const std::string &getVendor() const { return m_vendor; }
		inline const std::string &getFamily() const { return m_family; }
		inline const std::string &getDevice() const { return m_device; }

		virtual void fromConfig(const gtry::utils::ConfigTree &configTree);
	protected:
		std::string m_vendor;
		std::string m_family;
		std::string m_device;

		std::unique_ptr<EmbeddedMemoryList> m_embeddedMemoryList;
		GenericMemoryCapabilities m_memoryCapabilities;
		FifoCapabilities m_defaultFifoCaps; // for now
};

}