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

#include "StackTrace.h"
#include "Preprocessor.h"

#include <boost/lexical_cast.hpp>

#include <stdexcept>
#include <csignal>
#include <iostream>


namespace gtry::utils {

std::string composeMHDLErrorString(const char *file, size_t line, const std::string &what);


template<class BaseError>
class MHDLError : public BaseError
{
	public:
		MHDLError(const char *file, size_t line, const std::string &what) : 
				BaseError(composeMHDLErrorString(file, line, what)) {
					
			m_trace.record(20, 1);
		}		
		inline const StackTrace &getStackTrace() const { return m_trace; }
	protected:
		StackTrace m_trace;
};

extern template class MHDLError<std::logic_error>;
extern template class MHDLError<std::runtime_error>;

	
class InternalError : public MHDLError<std::logic_error>
{
	public:
		InternalError(const char *file, size_t line, const std::string &what);
		~InternalError();
};



class DesignError : public MHDLError<std::runtime_error>
{
	public:
		DesignError(const char *file, size_t line, const std::string &what);
		~DesignError();
};


template<class BaseError>
std::ostream &operator<<(std::ostream &stream, const MHDLError<BaseError> &exception) {
	/// @todo change format so that tools can more easily grab filename and line number
	stream 
		<< exception.what() << std::endl
		<< "Stack trace: " << std::endl
		<< exception.getStackTrace();
		
	return stream;
}

}
