/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "VpiModule.h"

#include <vpi_user.h>

vpi_host::VpiModule::VpiModule()
{
    vpiHandle mod_it = vpi_iterate(vpiModule, nullptr);
    m_VpiModule = vpi_scan(mod_it);
    vpi_free_object(mod_it);

    m_Info.rootModule = vpi_get_str(vpiName, m_VpiModule);
    m_Info.timeScale = (uint32_t)vpi_get(vpiTimePrecision, nullptr);

    initPorts();
}

void vpi_host::VpiModule::initPorts()
{
    vpiHandle net_it = vpi_iterate(vpiNet, m_VpiModule);
    while (vpiHandle net = vpi_scan(net_it))
    {
        vpi_host::SignalInfo sig;
        sig.name = vpi_get_str(vpiName, net);

        switch (vpi_get(vpiDirection, net))
        {
        case vpiInput:
            sig.direction = vpi_host::SignalDirection::in;
            break;
        case vpiOutput:
            sig.direction = vpi_host::SignalDirection::out;
            break;
        default:
            sig.direction = vpi_host::SignalDirection::none;
        }

        sig.width = (uint32_t)vpi_get(vpiWidth, net);

        if (sig.direction == vpi_host::SignalDirection::in)
        {
            m_Info.input.push_back(sig);
            m_InputNet.push_back(net);
        }
        else if (sig.direction == vpi_host::SignalDirection::out)
        {
            m_Info.output.push_back(sig);
            m_OutputNet.push_back(net);
        }
    }
}
