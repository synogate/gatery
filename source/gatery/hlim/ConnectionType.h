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
#ifndef CONNECTIONTYPE_H
#define CONNECTIONTYPE_H

#include <vector>
#include <string>
#include <memory>

/*
namespace gtry::sim {
	class DefaultBitVectorState;
}
*/

namespace gtry::hlim {

class Interpretation {
	public:
		virtual ~Interpretation() = default;

/*		
		virtual void translate(std::ostream &stream, const sim::DefaultBitVectorState &state) = 0;
		std::string translate(const sim::DefaultBitVectorState &state) {
			std::stringstream str;
			translate(str, state);
			return str.str();
		}
*/		
	protected:
};

struct ConnectionType
{
	enum Type {
		BOOL,
		BITVEC,
		DEPENDENCY,
	};
	
	Type type = BITVEC;

	Interpretation *interpretation = nullptr;
	
	size_t width = 0;
	
	bool operator==(const ConnectionType &rhs) const;
	bool operator!=(const ConnectionType &rhs) const { return !(*this == rhs); }

	bool isBool() const { return type == BOOL; }
	bool isBitVec() const { return type == BITVEC; }
	bool isDependency() const { return type == DEPENDENCY; }
};

}

#endif // CONNECTIONTYPE_H
