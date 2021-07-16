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

#include "Package.h"

#include "../../simulation/BitVectorState.h"
#include "../../frontend/Constant.h"

#include <map>
#include <string>
#include <sstream>

namespace gtry::vhdl {

class InterfacePackageContent {
    public:
		template<typename T>
		void addBVecConstant(const T &c, std::string_view comment = "");

		template<typename T>
		void addBitConstant(const T &c, std::string_view comment = "");

		void addInteger(std::string name, int value, std::string_view comment = "") { m_integerConstants[std::move(name)] = IntegerConstant{.value = value, .comment = std::string(comment)}; }

		struct IntegerConstant {
			int value;
			std::string comment;
		};
		struct BVecConstant {
			std::string value;
			size_t width;
			std::string comment;
		};
		struct BitConstant {
			std::string value;
			std::string comment;
		};

		inline void setName(std::string name) { m_name = std::move(name); }
		inline const std::string &getName() const { return m_name; }

		inline bool empty() { return m_integerConstants.empty() && m_BVecConstants.empty() && m_BitConstants.empty(); }

		inline const std::map<std::string, IntegerConstant> &getIntegerConstants() const { return m_integerConstants; }
		inline const std::map<std::string, BVecConstant> &getBVecConstants() const { return m_BVecConstants; }
		inline const std::map<std::string, BitConstant> &getBitConstants() const { return m_BitConstants; }
    protected:
		std::string m_name = "interface_package";
		std::map<std::string, IntegerConstant> m_integerConstants;
		std::map<std::string, BVecConstant> m_BVecConstants;
		std::map<std::string, BitConstant> m_BitConstants;
};


template<typename T>
void InterfacePackageContent::addBVecConstant(const T &c, std::string_view comment)
{
	auto v = parseBVec(c.getValue());

	std::stringstream v_str;
	v_str << '"' << v << '"';	

	m_BVecConstants[c.getName()] = BVecConstant{v_str.str(), v.size(), std::string(comment)};
}

template<typename T>
void InterfacePackageContent::addBitConstant(const T &c, std::string_view comment)
{
	auto v = parseBVec(c.getValue());

	std::stringstream v_str;
	v_str << '\'' << v << '\'';	

	m_BitConstants[c.getName()] = BitConstant{v_str.str(), std::string(comment)};
}


class InterfacePackage : public Package {
    public:
        InterfacePackage(AST &ast, const InterfacePackageContent &content);

        virtual void writeVHDL(std::ostream &stream) override;
    protected:
		const InterfacePackageContent &m_content;
};


}
