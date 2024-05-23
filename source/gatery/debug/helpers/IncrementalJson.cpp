/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2024 Michael Offel, Andreas Ley

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

#include "IncrementalJson.h"

namespace gtry::dbg::json {

IncrementalArray::Appender::Appender(IncrementalArray &file, std::ostream &stream)
		: m_file(file), m_stream(stream)
{
	m_file.startAppending();
}

IncrementalArray::Appender::~Appender()
{
	m_file.endAppending();
}

IncrementalArray::Appender &IncrementalArray::Appender::newEntity()
{
	if (m_file.m_empty)
		m_file.m_empty = false;
	else
		m_stream << ',';
	return *this;
}


IncrementalArray::IncrementalArray(const std::filesystem::path &filename, std::string_view arrayName)
{
	open(filename, arrayName);
}

void IncrementalArray::open(const std::filesystem::path &filename, std::string_view arrayName)
{
	m_filename = filename;
	m_stream.exceptions(std::fstream::badbit | std::fstream::failbit);
	m_stream.open(m_filename.string(), std::ios::binary | std::fstream::out | std::fstream::trunc);

	m_stream << "var " << arrayName << " = [\n]" << std::flush;
}


IncrementalArray::Appender IncrementalArray::append()
{
	return Appender(*this, m_stream);
}

void IncrementalArray::startAppending()
{
	// Remove closing ']'
	m_stream.seekp(-1, std::ios_base::cur);
}

void IncrementalArray::endAppending()
{
	m_stream << ']' << std::flush;
}


}
