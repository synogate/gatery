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
#pragma once

namespace gtry::utils {

class FileSink
{
	public:
		virtual ~FileSink() = default;
		virtual std::ostream &stream() = 0;
	protected:
};

class FileSystem
{
	public:
		virtual ~FileSystem() = default;
		
		virtual std::unique_ptr<FileSink> writeFile(const std::filesystem::path &filename, bool overwriteIfExists = true) = 0;
	protected:
};


class DiskFileSink : public FileSink
{
	public:
		DiskFileSink(const std::filesystem::path &filename, bool onlyWriteIfChanged, bool overwriteIfExists);
		virtual ~DiskFileSink() override;

		virtual std::ostream &stream() override;
	protected:
		std::filesystem::path m_filename;
		std::optional<std::ofstream> m_directToFileStream;
		std::optional<std::stringstream> m_memoryStream;
		bool m_discard = false;
};


class DiskFileSystem : public FileSystem
{
	public:
		DiskFileSystem(std::filesystem::path &basePath, bool onlyWriteIfChanged = false);

		virtual std::unique_ptr<FileSink> writeFile(const std::filesystem::path &filename = {}, bool overwriteIfExists = true) override;

		inline const std::filesystem::path &basePath() const { return m_basePath; }
	protected:
		std::filesystem::path m_basePath;
		bool m_onlyWriteIfChanged;
};


}
