#pragma once
#include "vpi_simulation_host.h"

#include <string_view>
#include <vector>
#include <optional>
#include <boost/process.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

namespace vpi_client
{
	namespace bp = boost::process;
	namespace fs = boost::filesystem;
	namespace ipc = boost::interprocess;

	using GenericsVector = std::vector<std::pair<std::string_view, std::string_view>>;

	class GhdlSimulation
	{
	public:
		GhdlSimulation();
		~GhdlSimulation();

		void launch(std::string_view topEntity, const GenericsVector& generics);
		int exit();

		const vpi_host::SimInfo& info() const { return m_SimInfo; }

	protected:
		void loadSimulationInfo();

		template<typename T>
		T loadResponse();

	private:
		const std::string m_InstanceName;
		bp::child m_GhdlProcess;
		std::optional<ipc::message_queue> m_CmdQueueP2C;
		std::optional<ipc::message_queue> m_CmdQueueC2P;

		vpi_host::SimInfo m_SimInfo;
		
	};


}
