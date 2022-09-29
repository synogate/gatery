/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

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

#include <sstream>
#include <string>

namespace gtry {

/**
 * @addtogroup gtry_frontend
 * @{
 */
	
class Comments
{
	public:
		static std::stringstream &get() { return m_comments; }
		static std::string retrieve() { std::string s = m_comments.str(); m_comments.str(std::string()); return s; }
	protected:
		static thread_local std::stringstream m_comments;
};

#define HCL_COMMENT gtry::Comments::get()

/// @}

}

