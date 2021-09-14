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

#include "IntelDevice.h"

#include "../general/GenericMemory.h"

#include "M20K.h"
#include "MLAB.h"
#include "GLOBAL.h"



namespace gtry::scl::arch::intel {



void IntelDevice::setupArria10(std::string device)
{
	m_vendor = "intel";
	m_family = "Arria 10";
	m_device = std::move(device);

    m_embeddedMemoryList = std::make_unique<EmbeddedMemoryList>();
    m_embeddedMemoryList->add(std::make_unique<MLAB>(*this));
    m_embeddedMemoryList->add(std::make_unique<M20K>(*this));

    m_technologyMapping.addPattern(std::make_unique<EmbeddedMemoryPattern>(*this));
    m_technologyMapping.addPattern(std::make_unique<GLOBALPattern>());
}

void IntelDevice::setupCyclone10(std::string device)
{
	m_vendor = "intel";
	m_family = "Cyclone 10";
	m_device = std::move(device);

    m_embeddedMemoryList = std::make_unique<EmbeddedMemoryList>();
    m_embeddedMemoryList->add(std::make_unique<MLAB>(*this));
    m_embeddedMemoryList->add(std::make_unique<M20K>(*this));

    m_technologyMapping.addPattern(std::make_unique<EmbeddedMemoryPattern>(*this));
    m_technologyMapping.addPattern(std::make_unique<GLOBALPattern>());
}



}