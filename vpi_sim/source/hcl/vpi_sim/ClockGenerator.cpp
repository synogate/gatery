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
#include "ClockGenerator.h"

#include <vpi_user.h>
#include <string>

vpi_host::ClockGenerator::ClockGenerator(uint64_t clockSimInterval, void* vpiNetHandle) :
	m_Interval(clockSimInterval),
	m_VpiNet(vpiNetHandle),
	m_VpiCallback(nullptr)
{
	PLI_INT32 width = vpi_get(vpiWidth, m_VpiNet);
	if (width != 1)
	{
		std::string name = vpi_get_str(vpiFullName, m_VpiNet);
		vpi_printf("warning: %s is not a scalar signal but used as clock\n", name.c_str());
	}
	else
	{
		onTimeInterval();
	}
}

vpi_host::ClockGenerator::~ClockGenerator()
{
	if (m_VpiCallback)
		vpi_remove_cb(m_VpiCallback);
}

static PLI_INT32 onClockDelayReached(t_cb_data* data)
{
	auto* clock = (vpi_host::ClockGenerator*)data->user_data;
	clock->onTimeInterval();
	return 0;
}

void vpi_host::ClockGenerator::onTimeInterval()
{
	t_vpi_value val{ vpiIntVal };
	t_vpi_time delay{ vpiSimTime };

	uint64_t half_interval = m_Interval / 2;
	delay.low = (uint32_t)half_interval;
	delay.high = half_interval >> 32;
	val.value.integer = 0;
	vpi_put_value(m_VpiNet, &val, &delay, 0);

	delay.low = (uint32_t)m_Interval;
	delay.high = uint32_t(m_Interval >> 32);
	val.value.integer = 1;
	vpi_put_value(m_VpiNet, &val, &delay, 0);

	t_cb_data cb{ cbAfterDelay, &onClockDelayReached };
	cb.time = &delay;
	cb.user_data = (PLI_BYTE8*)this;
	m_VpiCallback = vpi_register_cb(&cb);
}
