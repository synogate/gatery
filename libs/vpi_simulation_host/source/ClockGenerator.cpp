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
