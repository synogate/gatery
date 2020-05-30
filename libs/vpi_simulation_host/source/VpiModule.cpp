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
