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
#include <gatery/hlim/postprocessing/TechnologyMapping.h>

#include <string>
#include <vector>

namespace gtry::scl::arch {

class GenericMemoryCapabilities;

class TargetDevice {
	public:
        TargetDevice();
        inline const std::vector<GenericMemoryDesc> &getEmbeddedMemories() const { return m_embeddedMemories; }
        inline const hlim::TechnologyMapping &getTechnologyMapping() const { return m_technologyMapping; }

		inline const std::string &getVendor() const { return m_vendor; }
		inline const std::string &getFamily() const { return m_family; }
		inline const std::string &getDevice() const { return m_device; }

        TechnologyScope enterTechScope() { return TechnologyScope(m_techCaps); }
	protected:
		std::string m_vendor;
		std::string m_family;
		std::string m_device;

        TechnologyCapabilities m_techCaps;

        std::vector<GenericMemoryDesc> m_embeddedMemories;
        std::unique_ptr<GenericMemoryCapabilities> m_memoryCapabilities;
        hlim::TechnologyMapping m_technologyMapping;
};

}