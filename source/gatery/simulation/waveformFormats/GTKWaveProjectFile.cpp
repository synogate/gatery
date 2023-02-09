/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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

#include <gatery/frontend/Enum.h>
#include "GTKWaveProjectFile.h"

#include <fstream>
#include <stdexcept>

namespace gtry::sim {

void GTKWaveProjectFile::setWaveformFile(std::string filename)
{ 
	m_waveformFile = std::move(filename);
}

void GTKWaveProjectFile::setZoom(const hlim::ClockRational &start, const hlim::ClockRational &end)
{
	m_timestart = (std::uint64_t)( start.numerator() * 1e12 / start.denominator() );

	hlim::ClockRational range = end - start;
	if (range == hlim::ClockRational{0,1})
		m_zoom = -17.0;
	else {	
		// 2^-17 ~~ 1us
		double range_us = range.numerator() * 1e6 / range.denominator();
		m_zoom = float(-17.0 - std::log2(range_us));
	}
}

void GTKWaveProjectFile::appendBlank()
{
	m_signals.push_back({
		.format = Signal::BLANK_LINE,
	});
}

void GTKWaveProjectFile::appendComment(std::string comment)
{
	m_signals.push_back({
		.format = Signal::COMMENT,
		.alias = std::move(comment),
	});
}

GTKWaveProjectFile::Signal &GTKWaveProjectFile::appendSignal(std::string signalName)
{
	m_signals.push_back({
		.signalName = std::move(signalName),
	});
	return m_signals.back();
}

GTKWaveProjectFile::Marker* GTKWaveProjectFile::addMarker(const hlim::ClockRational &time)
{
	for (auto &m : m_markers) {
		if (m.time_ps == -1) {
			m.time_ps = (std::int64_t)( time.numerator() * 1e12 / time.denominator() );
			return &m;
		}
	}
	return nullptr;
}

void GTKWaveProjectFile::setCursor(const hlim::ClockRational &time)
{
	m_cursor_ps = (std::int64_t)( time.numerator() * 1e12 / time.denominator() );
}

void GTKWaveProjectFile::write(const char *filename) const
{
	std::fstream file(filename, std::fstream::out | std::fstream::binary);
	file 
		<< "[*]\n[*] Exported from Gatery\n[*]\n"
		<< "[dumpfile] \"" << m_waveformFile << "\"\n"
		<< "[savefile] \"" << filename << "\"\n"
		<< "[timestart] " << m_timestart << '\n'
		<< "[size] 1920 1027\n"
		<< "[pos] -1 -1\n"
		<< '*' << m_zoom << ' ' << m_cursor_ps;
	for (const auto &m : m_markers)
		file << ' ' << m.time_ps;
	file << '\n';

	for (const auto &s : m_signals) {
		file << '@' << (size_t) s.format << '\n';
		switch (s.format) {
			case Signal::BLANK_LINE:
				file << "-\n";
			break;
			case Signal::COMMENT:
				file << '-' << s.alias << '\n';
			break;
			default:
				if (s.color != Signal::NORMAL)
					file << "[color] " << (size_t)s.color << '\n';
				if (!s.alias.empty())
					file << "+{" << s.alias << "} ";
				file << s.signalName << '\n';
			break;
		}
	}

	for (const auto& e : m_translationFilterFiles)
		file << "^1 " << e << '\n';
}

void GTKWaveProjectFile::writeEnumFilterFiles()
{
	for (const auto& e : KnownEnum::knwonEnums())
	{
		std::string filename = getWaveformFile() + "." + e.first + ".filter";
		std::ofstream f{ filename };
		for (auto&& val : e.second)
			f << val.first << ' ' << val.second << '\n';
		m_translationFilterFiles.emplace_back(move(filename));
	}
}

}
