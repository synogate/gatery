#include "gatery/pch.h"
#include "StreamMasterModel.h"
#include "Packet.h"

namespace gtry::scl
{
	void In(scl::StreamSignal auto& stream, std::string prefix = "in")
	{
		pinIn(stream, prefix);
	}
	void PacketStreamMasterModel::init(BitWidth payloadW, bool debug)
	{
		Clock clk = ClockScope::getClk();

		//tileLinkInit(m_link, addrWidth, dataWidth, sizeWidth, sourceWidth);
		*in = payloadW;
		In(in);

		if (debug) {
			m_rng.seed(29857);//fixed seed for debugging
		}
		else {
			m_rng.seed(std::random_device{}());
		}

		// valid chaos monkey, but only for the start of a packet
		DesignScope::get()->getCircuit().addSimulationProcess([this, clk]() -> SimProcess {
			simu(valid(in)) = '0';
			bool idle = true;

			while (true)
			{
				co_await OnClk(clk);
				if (in->eop) {
					idle = 1;
				}
				if (idle) {
					bool isValid = (m_dis(m_rng) <= m_validProbability);
					simu(valid(in)) = isValid;
					idle = !isValid;
				}
			}
			});
	}
	void PacketStreamMasterModel::probability(float valid){
		m_validProbability = valid;
	}
}

