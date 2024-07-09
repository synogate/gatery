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
#include "gatery/pch.h"
#include "Exceptions.h"

namespace gtry::utils {

template class MHDLError<std::logic_error>;
template class MHDLError<std::runtime_error>;

std::string composeMHDLErrorString(const char *file, size_t line, const std::string &what)
{
	return what + " Location: " + file + '('+boost::lexical_cast<std::string>(line)+')';
}

InternalError::InternalError(const char *file, size_t line, const std::string &what) : MHDLError<std::logic_error>(file, line, what) { }
InternalError::~InternalError() { }

DesignError::DesignError(const char *file, size_t line, const std::string &what) : MHDLError<std::runtime_error>(file, line, what) {
#ifdef _WIN32
	OutputDebugStringA(what.c_str());
#endif
}
DesignError::~DesignError() { }

}
