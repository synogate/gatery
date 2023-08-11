/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "gatery/pch.h"
#include "FileSystem.h"

namespace gtry::utils {

	DiskFileSink::DiskFileSink(const std::filesystem::path &filename, bool onlyWriteIfChanged, bool overwriteIfExists) : m_filename(filename)
	{
		if (!onlyWriteIfChanged || !std::filesystem::exists(m_filename)) {
			auto parentPath = m_filename.parent_path();
			if (!parentPath.empty())
				std::filesystem::create_directories(parentPath);

			m_directToFileStream.emplace();
			m_directToFileStream->exceptions(std::fstream::failbit | std::fstream::badbit);
			m_directToFileStream->open(m_filename.string().c_str(), std::fstream::out | std::fstream::binary);
		} else {
			m_discard = !overwriteIfExists && std::filesystem::exists(m_filename);
			m_memoryStream.emplace();
		}
	}

	DiskFileSink::~DiskFileSink()
	{
		if (m_memoryStream && !m_discard) {
			std::ifstream input;
			input.exceptions(std::fstream::badbit);
			input.open(m_filename.string().c_str(), std::fstream::in | std::fstream::binary);

			bool same = true;
			for (const auto &c : m_memoryStream->str()) {
				auto file_c = input.get();
				if (!input || file_c != c) {
					same = false;
					break;
				}
			}

			input.close();

			if (!same) {
				std::ofstream output;
				output.exceptions(std::fstream::failbit | std::fstream::badbit);
				output.open(m_filename.string().c_str(), std::fstream::out | std::fstream::binary);
				output.write(m_memoryStream->str().c_str(), m_memoryStream->str().length());
			}
		}
	}

	std::ostream &DiskFileSink::stream()
	{
		if (m_memoryStream)
			return *m_memoryStream;
		return *m_directToFileStream;
	}

	DiskFileSystem::DiskFileSystem(std::filesystem::path &basePath, bool onlyWriteIfChanged) : m_basePath(basePath), m_onlyWriteIfChanged(onlyWriteIfChanged)
	{

	}

	std::unique_ptr<FileSink> DiskFileSystem::writeFile(const std::filesystem::path &filename, bool overwriteIfExists)
	{
		if (!m_basePath.empty())
			return std::make_unique<DiskFileSink>(m_basePath / filename, m_onlyWriteIfChanged, overwriteIfExists);
		else
			return std::make_unique<DiskFileSink>(filename, m_onlyWriteIfChanged, overwriteIfExists);
	}
}