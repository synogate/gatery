#include "gatery/scl_pch.h"
#include "URAM288.h"

namespace gtry::scl::arch::xilinx
{
	URAM288::URAM288() :
		ExternalModule("URAM288", "UNISIM", "vcomponents")
	{
		in("SLEEP") = '0';

		port(A, { .en = '0' });
		port(B, { .en = '0' });

		for (std::string port : { "_A", "_B" })
		{
			in("INJECT_SBITERR" + port) = '0';
			in("INJECT_DBITERR" + port) = '0';
			in("OREG_CE" + port) = '1';
			in("OREG_ECC_CE" + port) = '1';
			in("RST" + port) = '0';

			in("CAS_IN_ADDR" + port, 23_b);
			in("CAS_IN_BWE" + port, 9_b);
			in("CAS_IN_DBITERR" + port);
			in("CAS_IN_DIN" + port, 72_b);
			in("CAS_IN_DOUT" + port, 72_b);
			in("CAS_IN_EN" + port);
			in("CAS_IN_RDACCESS" + port);
			in("CAS_IN_RDB_WR" + port);
			in("CAS_IN_SBITERR" + port);
		}
	}

	void URAM288::clock(const Clock& clock)
	{
		clockIn(clock, "CLK");
		// TODO: connect reset signals
	}

	URAM288::PortOut URAM288::port(Port portId)
	{
		std::string suffix = portId == Port::A ? "_A" : "_B";

		return {
			.dout = out("DOUT" + suffix, 72_b),
			.sbiterr = out("SBITERR" + suffix),
			.dbiterr = out("DBITERR" + suffix),
			.rdaccess = out("RDACCESS" + suffix)
		};
	}

	void URAM288::port(Port portId, const PortIn& portIn)
	{
		std::string suffix = portId == Port::A ? "_A" : "_B";
		in("DIN" + suffix, 72_b) = portIn.din;
		in("ADDR" + suffix, 23_b) = (BVec)portIn.addr;
		in("EN" + suffix) = portIn.en;
		in("RDB_WR" + suffix) = portIn.rdb_wr;
		in("BWE" + suffix, 9_b) = portIn.bwe;
	}

	void URAM288::cascade(URAM288& inRam, size_t numRamsInTotal)
	{
		m_cascadeAddress = inRam.m_cascadeAddress + 1;
		size_t selfMask = (~0ull << utils::Log2C(numRamsInTotal)) & 0x7FF;

		for (std::string port : { "_A", "_B" })
		{
			std::string inCascadeOrder = inRam.m_cascadeAddress == 0 ? "FIRST" : "MIDDLE";
			inRam.generic("CASCADE_ORDER" + port) = inCascadeOrder;
			inRam.generic("SELF_ADDR" + port).setBitVector(11, inRam.m_cascadeAddress);
			inRam.generic("SELF_MASK" + port).setBitVector(11, selfMask);

			generic("CASCADE_ORDER" + port) = "LAST";
			generic("SELF_ADDR" + port).setBitVector(11, m_cascadeAddress);
			generic("SELF_MASK" + port).setBitVector(11, selfMask);

			// map: addr, bwe, dbiterr, din, dout, en, rdaccess, rdb_wr, sbiterr
			in("CAS_IN_ADDR" + port, 23_b) = inRam.out("CAS_OUT_ADDR" + port, 23_b);
			in("CAS_IN_BWE" + port, 9_b) = inRam.out("CAS_OUT_BWE" + port, 9_b);
			in("CAS_IN_DBITERR" + port) = inRam.out("CAS_OUT_DBITERR" + port);
			in("CAS_IN_DIN" + port, 72_b) = inRam.out("CAS_OUT_DIN" + port, 72_b);
			in("CAS_IN_DOUT" + port, 72_b) = inRam.out("CAS_OUT_DOUT" + port, 72_b);
			in("CAS_IN_EN" + port) = inRam.out("CAS_OUT_EN" + port);
			in("CAS_IN_RDACCESS" + port) = inRam.out("CAS_OUT_RDACCESS" + port);
			in("CAS_IN_RDB_WR" + port) = inRam.out("CAS_OUT_RDB_WR" + port);
			in("CAS_IN_SBITERR" + port) = inRam.out("CAS_OUT_SBITERR" + port);
		}
	}

	void URAM288::cascadeReg(bool enableCascadingReg)
	{
		for (std::string port : { "_A", "_B" })
		{
			generic("REG_CAS" + port) = enableCascadingReg ? "TRUE" : "FALSE";
		}
	}

	void URAM288::enableOutputRegister(Port portId, bool enable)
	{
		std::string suffix = portId == Port::A ? "_A" : "_B";
		generic("OREG" + suffix) = enable ? "TRUE" : "FALSE";
	}
}
