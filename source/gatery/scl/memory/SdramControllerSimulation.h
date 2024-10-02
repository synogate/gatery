#pragma once
#include "sdram.h"

namespace gtry::scl::sdram
{
	class SdramControllerSimulation : public Controller
	{
	public:
		void makeBusPins(const scl::sdram::CommandBus& in, std::string prefix) override
		{
			Bit outEnable = m_dataOutEnable;
			scl::sdram::CommandBus bus = in;
			if (m_useOutputRegister)
			{
				bus = gtry::reg(in);
				bus.cke = gtry::reg(in.cke, '0');
				bus.dqm = gtry::reg(in.dqm, ConstBVec(0, in.dqm.width()));
				outEnable = gtry::reg(outEnable, '0');
			}

			pinOut(bus.cke).setName(prefix + "CKE");
			pinOut(bus.csn).setName(prefix + "CSn");
			pinOut(bus.rasn).setName(prefix + "RASn");
			pinOut(bus.casn).setName(prefix + "CASn");
			pinOut(bus.wen).setName(prefix + "WEn");
			pinOut(bus.a).setName(prefix + "A");
			pinOut(bus.ba).setName(prefix + "BA");
			pinOut(bus.dqm).setName(prefix + "DQM");
			pinOut(bus.dq).setName(prefix + "DQ_OUT");
			pinOut(outEnable).setName(prefix + "DQ_OUT_EN");

			BVec moduleData = *scl::sdram::moduleSimulation(bus);
			HCL_NAMED(moduleData);

			m_dataIn = ConstBVec(moduleData.width());
			IF(!outEnable)
				m_dataIn = moduleData;
			if (m_useInputRegister)
				m_dataIn = reg(m_dataIn);
			pinOut(m_dataIn).setName(prefix + "DQ_IN");
		}
	};
}
