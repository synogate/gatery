
#pragma once

#include "gatery/debug/DebugInterface.h"
#include "gatery/debug/websocks/WebSocksInterface.h"

#include "../helpers/IncrementalJson.h"

#include <memory>
#include <string>
#include <variant>
#include <span>
#include <iostream>
#include <fstream>

namespace gtry::utils {
	class StackTrace;
}

namespace gtry::hlim {
	class ConstSubnet;
	class Circuit;
}

/**
 * @addtogroup gtry_frontend_logging
 * @{
 */

namespace gtry::dbg {
	class ReportInterface : public DebugInterface
	{
		public:
			static void create(const std::filesystem::path &outputDir);

			ReportInterface(const std::filesystem::path &outputDir);

			virtual void log(LogMessage msg) override;

			virtual void changeState(State state, hlim::Circuit* circuit) override;

			virtual std::string howToReachLog() override;
		protected:
			std::filesystem::path m_outputDir;

			json::IncrementalArray m_logMessages;
			json::IncrementalArray m_nodes;
			json::IncrementalArray m_nodeGroups;
			json::IncrementalArray m_prerenderedSubnets;

			int m_image_counter = 0;

			static void populateDirWithStaticFiles(const std::filesystem::path &outputDir);

			void prerenderSubnet(const hlim::ConstSubnet &subnet);
	};
}

/**@}*/