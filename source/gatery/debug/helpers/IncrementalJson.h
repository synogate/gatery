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

#include <string_view>
#include <fstream>
#include <filesystem>

/**
 * @addtogroup gtry_frontend_logging
 * @{
 */

namespace gtry::dbg::json {

class IncrementalArray {	
	public:
		IncrementalArray() = default;
		IncrementalArray(const std::filesystem::path &filename, std::string_view arrayName);
		void open(const std::filesystem::path &filename, std::string_view arrayName);

		class Appender {
			public:
				Appender(IncrementalArray &file, std::ostream &stream);
				~Appender();

				Appender &newEntity();

				operator std::ostream&() { return m_stream; }
			protected:
				IncrementalArray &m_file;
				std::ostream &m_stream;
		};

		Appender append();
	protected:
		std::ofstream m_stream;
		std::filesystem::path m_filename;
		bool m_empty = true;

		void startAppending();
		void endAppending();

		friend class Appender;
};

}

/**@}*/