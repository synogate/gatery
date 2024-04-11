#include "gatery/pch.h"
#include "gatery/debug/reporting/ReportInterface.h"
#include "gatery/debug/helpers/JsonSerialization.h"
#include "gatery/utils/StackTrace.h"
#include "gatery/hlim/NodeGroup.h"
#include "gatery/hlim/Subnet.h"
#include "gatery/hlim/Node.h"
#include "gatery/frontend/DesignScope.h"
#include "external/magic_enum.hpp"
#include <iostream>
#include <fstream>
#include <utility>
#include <algorithm>
#include <string>
#include <string_view>

#include <res/gtry_resources.h>

#include <gatery/export/DotExport.h>

/*

# Auto reloading json:

Load json into invisible iframe:
<iframe src="https://www.youtube.com/embed/X18mUlDddCc?autoplay=1" style="position: absolute;width:0;height:0;border:0;"></iframe>


var iframe = document.getElementById("myframe");
var frame_content = (iframe.contentWindow || iframe.contentDocument);


auto-refresh iframe:
frame_content.location.reload();

read content from there:
var pre_info = frame_content.document.getElementsByTagName("pre")[0].innerHTML;
var item_info = JSON.parse(pre_info.slice(1, -1));

*/




using namespace std::literals;

namespace gtry::dbg {

	void ReportInterface::populateDirWithStaticFiles(const std::filesystem::path &outputDir)
	{
		for (const auto &resFile : res::manifest) {
			if (resFile.filename.starts_with("data/reporting/")) {
				auto filename = outputDir / resFile.filename.substr("data/reporting/"sv.length());
				auto folder = filename.parent_path();
				if (!std::filesystem::exists(folder))
					std::filesystem::create_directories(folder);

				std::ofstream file(filename.string().c_str(), std::fstream::binary);
				file.write((const char *)resFile.data.data(), resFile.data.size());
			}
		}
	}

	void ReportInterface::create(const std::filesystem::path &outputDir)
	{
		instance.reset(nullptr); // Close previous first
		instance.reset(new ReportInterface(outputDir));
	}

	ReportInterface::ReportInterface(const std::filesystem::path &outputDir) : m_outputDir(outputDir)
	{
		populateDirWithStaticFiles(outputDir);
		
		auto dataFolder = outputDir / "data";
		if (!std::filesystem::exists(dataFolder))
			std::filesystem::create_directories(dataFolder);		
		
		m_logMessages.open(dataFolder / "report.js", "logMessages"sv);
		m_nodes.open(dataFolder / "nodes.js", "hierarchyNodeData"sv);
		m_nodeGroups.open(dataFolder / "groups.js", "hierarchyGroupData"sv);
		m_prerenderedSubnets.open(dataFolder / "prerenderedSubnets.js", "prerenderedSubnets"sv);
	}

	std::string ReportInterface::howToReachLog()
	{
		return std::string("In a web browser, open file://") + std::filesystem::absolute(m_outputDir / "index.html").string();
	}

	void ReportInterface::log(LogMessage msg)
	{
		for (const auto &part : msg.parts())
			if (std::holds_alternative<hlim::Subnet>(part))
				prerenderSubnet(std::get<hlim::Subnet>(part).asConst());
	
		json::serializeLogMessage(m_logMessages.append().newEntity(), msg);
	}


	void ReportInterface::changeState(State state, hlim::Circuit* circuit) {
		if (state == State::POSTPROCESSINGDONE) {
			json::serializeGroup(m_nodeGroups.append(), circuit->getRootNodeGroup(), true);
			json::serializeAllNodes(m_nodes.append(), *circuit);
		}
	}

	std::string getFileContentAsString(const std::filesystem::path& path) {
		std::ifstream inputFile(path);
		if (!inputFile.is_open())
		{
			std::cerr << "Failed to open the file for reading." << std::endl;
		}

		std::stringstream out;
		out << inputFile.rdbuf();
		return out.str();
	}

	std::string replaceAll(std::string svgAsText, const std::string& delimiter, const std::string& insert) {
		size_t pos = 0;
		while ((pos = svgAsText.find(delimiter, pos)) != std::string::npos) {
			svgAsText.replace(pos, delimiter.length(), insert);
			pos += insert.length();
		}
		return svgAsText;
	}	

	std::string escapeAndAddId(std::string svgAsText, size_t id) {
		std::string delimiter = " ";
		size_t pos = 0;
		std::string token;
		std::string insert = " id=\"svg-object-" + std::to_string(id) + "\" ";
		if ((pos = svgAsText.find(delimiter)) != std::string::npos) {
			svgAsText.replace(pos, delimiter.length(), insert);
		}
		svgAsText = replaceAll(svgAsText, "\n", "\\\n");
		svgAsText = replaceAll(svgAsText, "\"", "\\\"");
		return svgAsText;
	}

	std::string removeHeader(const std::string& svgAsText) {
		std::string delimiter = "<svg";
		size_t pos = 0;
		if ((pos = svgAsText.find(delimiter)) != std::string::npos) {
			return svgAsText.substr(pos, svgAsText.length());
		}
		return "";
	}


	void prerenderedSubnetSvgToJson(std::ostream &json, size_t id, const hlim::ConstSubnet &subnet, const std::filesystem::path &prerenderedSvg)
	{
		std::string cleanedSvgAsText = escapeAndAddId(removeHeader(getFileContentAsString(prerenderedSvg)), id);

		json << "{ \"imageId\" : " << id << ",\n  \"subnet_nodes\": ";
		json::serializeSubnet(json, subnet);
		json << ",\n \"content\": \"" << cleanedSvgAsText << "\"}";
	}


	void ReportInterface::prerenderSubnet(const hlim::ConstSubnet &subnet)
	{
		if (subnet.empty()) return;

		auto *circuit = (*subnet.begin())->getCircuit();

		DotExport exp("temp_prerendered_subnet.dot");
		exp(*circuit, subnet.asConst());
		exp.runGraphViz("temp_prerendered_subnet.svg");

		prerenderedSubnetSvgToJson(m_prerenderedSubnets.append().newEntity(), m_image_counter++, subnet, "temp_prerendered_subnet.svg");
	}
}
