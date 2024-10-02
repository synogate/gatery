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
#pragma once

#include "../../hlim/ClockRational.h"

#include <map>
#include <string>
#include <vector>

namespace gtry::hlim {

class Clock;

}

namespace gtry::sim {

class GTKWaveProjectFile
{
	public:
		struct Signal {
			enum Format {
				HEX = 22,
				DEC = 25,
				BIN = 29,
				ASCII = 821,
				BLANK_LINE = 200,
				COMMENT = 201,
			};
			enum Color {
				NORMAL = 0,
				RED = 1,
				ORANGE = 2,
				YELLOW = 3,
				GREEN = 4,
				BLUE = 5,
				INDIGO = 6,
				VIOLET = 7,
			};
			Format format = HEX;
			Color color = NORMAL;
			std::string alias;
			std::string signalName;
		};

		struct Marker {
			std::int64_t time_ps = -1;
		};

		void setWaveformFile(std::string filename);
		const std::string &getWaveformFile() const { return m_waveformFile; }
		void setZoom(const hlim::ClockRational &start, const hlim::ClockRational &end);

		void appendBlank();
		void appendComment(std::string comment);
		Signal &appendSignal(std::string signalName);
		Marker *addMarker(const hlim::ClockRational &time);
		void setCursor(const hlim::ClockRational &time);

		void write(const char *filename) const;
		void writeSurferScript(const std::filesystem::path& filename) const;
		void writeEnumFilterFiles();
	protected:
		std::string m_waveformFile;
		std::int64_t m_cursor_ps = -1;
		std::uint64_t m_timestart = 0;
		float m_zoom = -20.0;

		std::array<Marker, 26> m_markers;

		std::vector<Signal> m_signals;
		std::vector<std::string> m_translationFilterFiles;
};


}
