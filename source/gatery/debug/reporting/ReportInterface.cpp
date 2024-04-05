#include "gatery/pch.h"
#include "gatery/debug/reporting/ReportInterface.h"
#include "gatery/utils/StackTrace.h"
#include "gatery/hlim/NodeGroup.h"
#include "gatery/frontend/DesignScope.h"
#include "external/magic_enum.hpp"
#include <iostream>
#include <fstream>
#include <utility>
#include <algorithm>
#include <string>
#include <string_view>

#include <res/gtry_resources.h>

using namespace std::literals;

namespace gtry::dbg {

	void ReportInterface::populateDirWithStaticFiles(const std::filesystem::path &outputDir)
	{
		if (!std::filesystem::exists(outputDir))
			std::filesystem::create_directories(outputDir);

		for (const auto &resFile : res::manifest) {
			if (resFile.filename.starts_with("data/reporting/")) {
				auto filename = outputDir / resFile.filename.substr("data/reporting/"sv.length());
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
	}

	void ReportInterface::appendJsonToFile(const std::string& toAppend, std::filesystem::path path, bool create)
	{
		if (create)
		{
			std::ofstream jsonFile(path);
			jsonFile.close();
		}

		std::string fileContent = getFileContentAsString(path);

		if (fileContent.empty() || fileContent.size() < 2)
		{
			fileContent = "var logMessages = []";
		}

		fileContent.pop_back();
		std::ofstream outputFile(path);
		if (!outputFile.is_open())
		{
			std::cerr << "Failed to open the file for writing." << std::endl;
		}

		outputFile << fileContent;
		if (!create) {
			outputFile << "," << toAppend << "]";
		} else {
			outputFile << toAppend << "]";
		}

		outputFile.close();
	}

	void ReportInterface::appendSvgJsonToFile(const std::string& toAppend, std::filesystem::path path, bool create)
	{
		if (create)
		{
			std::ofstream jsonFile(path);
			jsonFile.close();
		}

		std::string fileContent = getFileContentAsString(path);

		if (fileContent.empty() || fileContent.size() < 2)
		{
			fileContent = "var loopSVGs = []";
		}

		fileContent.pop_back();
		std::ofstream outputFile(path);
		if (!outputFile.is_open())
		{
			std::cerr << "Failed to open the file for writing." << std::endl;
		}

		outputFile << fileContent;
		if (!create) {
			outputFile << "," << toAppend << "]";
		} else {
			outputFile << toAppend << "]";
		}

		outputFile.close();
	}

	void ReportInterface::addToJson(const LogMessage& msg)
	{
		std::filesystem::path location = m_outputDir / "report.js";

		if (m_first_message)
		{
			// first log message received
			appendJsonToFile(serializeLogMessage(msg), location, m_first_message);
			m_first_message = false;
		}
		else
		{
			// not first log message received
			appendJsonToFile(serializeLogMessage(msg), location, m_first_message);
		}

	}

	void ReportInterface::log(LogMessage msg)
	{
		addToJson(msg);
	}

	void ReportInterface::svgToText(const std::filesystem::path& src)
	{
		std::filesystem::path location = m_outputDir / "loopSVG.js";
		std::string svgAsText = getFileContentAsString(std::move(src));

		std::string cleanedSvgAsText = cleanSvgText(svgAsText);
		std::string cleanedSvgAsJson = svgTextToJson(cleanedSvgAsText);
		if (m_first_image) {
			appendSvgJsonToFile(cleanedSvgAsJson, location, m_first_image);
			m_first_image = false;
		} else {
			appendSvgJsonToFile(cleanedSvgAsJson, location, m_first_image);
		}
	}

	// migrated from HierarchyInterface
	void ReportInterface::hierarchyJSON(hlim::NodeGroup* RootGroup) {

		if (RootGroup == nullptr) {
			std::cerr << "RootGroup is nullptr" << std::endl;
			return;
		}

		std::ofstream jsonGroupsFile(m_outputDir / "serializedGroups.js");
		jsonGroupsFile << "var hierarchyGroupData = [\n";
		jsonGroupsFile << JsonSerializer::serializeAllGroups(RootGroup->getCircuit()) << "]";
		jsonGroupsFile.close();

		std::ofstream jsonNodesFile(m_outputDir / "serializedNodes.js");
		jsonNodesFile << "var hierarchyNodeData = [\n";
		jsonNodesFile << JsonSerializer::serializeAllNodes(RootGroup->getCircuit()) << "]";
		jsonNodesFile.close();
	}

	void ReportInterface::changeState(State state, hlim::Circuit* circuit) {
		if (state == State::POSTPROCESSINGDONE) {
			//hierarchyBFS(circuit->getRootNodeGroup());
			hierarchyJSON(circuit->getRootNodeGroup());
		}
	}

	std::string ReportInterface::getFileContentAsString(const std::filesystem::path& path) {
		std::ifstream inputFile(path);
		if (!inputFile.is_open())
		{
			std::cerr << "Failed to open the file for reading." << std::endl;
		}

		std::string fileContents;
		char token;
		while (inputFile.get(token))
		{
			fileContents += token;
		}

		inputFile.close();
		return fileContents;
	}

	std::string ReportInterface::cleanSvgText(const std::string& svgAsText) {
		std::string delimiter = "<svg";
		size_t pos = 0;
		std::string token;
		if ((pos = svgAsText.find(delimiter)) != std::string::npos) {
			token = svgAsText.substr(pos, svgAsText.length());
		}
		return prepSvgText(token);
	}

	std::string ReportInterface::prepSvgText(std::string svgAsText) {
		std::string delimiter = " ";
		size_t pos = 0;
		std::string token;
		const std::string& insert = " id=\"svg-object-" + std::to_string(m_image_counter) + "\" ";
		if ((pos = svgAsText.find(delimiter)) != std::string::npos) {
			svgAsText.replace(pos, delimiter.length(), insert);
		}
		svgAsText = ReportInterface::replaceAll(svgAsText, "\n", "\\\n");
		svgAsText = ReportInterface::replaceAll(svgAsText, "\"", "\\\"");
		return svgAsText;
	}

	std::string ReportInterface::replaceAll(std::string svgAsText, const std::string& delimiter, const std::string& insert) {
		size_t pos = 0;
		while ((pos = svgAsText.find(delimiter, pos)) != std::string::npos) {
			svgAsText.replace(pos, delimiter.length(), insert);
			pos += insert.length();
		}
		return svgAsText;
	}

	std::string ReportInterface::svgTextToJson(const std::string& svgAsText) {
		std::string jsonObject;
		jsonObject = R"({"image)" + std::to_string(m_image_counter) + R"(":")" + svgAsText + R"("})";
		m_image_counter++;
		return jsonObject;
	}
}
