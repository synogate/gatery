
#pragma once

#include "gatery/debug/DebugInterface.h"
#include "gatery/debug/websocks/WebSocksInterface.h"


#include <memory>
#include <string>
#include <variant>
#include <span>
#include <iostream>
#include <fstream>

namespace gtry::utils {
	class StackTrace;
}

namespace gtry::dbg {
	class ReportInterface : public DebugInterface, public JsonSerializer
	{
		public:
			static void create(const std::filesystem::path &outputDir);

			ReportInterface(const std::filesystem::path &outputDir);

			void log(LogMessage msg) override;
			void addToJson(const LogMessage& msg);
			void appendJsonToFile(const std::string& toAppend, std::filesystem::path path, bool create);
			void appendSvgJsonToFile(const std::string& toAppend, std::filesystem::path path, bool create);
			void svgToText(const std::filesystem::path& src);

			virtual void changeState(State state, hlim::Circuit* circuit) override;

			//migrated from HierarchyInterface
			void hierarchyBFS(hlim::NodeGroup *RootGroup);
			void hierarchyJSON(hlim::NodeGroup *RootGroup);

			std::string getFileContentAsString(const std::filesystem::path& path);
			std::string cleanSvgText(const std::string& svgAsText);
			std::string svgTextToJson(const std::string& svgAsText);
			std::string prepSvgText(std::string svgAsText);
			std::string replaceAll(std::string svgAsText, const std::string& delimiter, const std::string& insert);

		protected:
			std::filesystem::path m_outputDir;

			std::vector<std::string> m_logMessages;
			bool m_first_message = true;
			bool m_first_image = true;
			int m_image_counter = 0;

			//migrated from HierarchyInterface
			struct QueueElement {
				hlim::NodeGroup *nodeGroup = nullptr;
			};
			// node groups in this vector are ordered as follows: a node group is either siblings with the node group to its right or its parent.
			std::deque<QueueElement> m_nodegroup_hierarchy;

			static void populateDirWithStaticFiles(const std::filesystem::path &outputDir);

	};
}

