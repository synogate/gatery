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

#include "../Node.h"

#include <vector>
#include <string>
#include <map>

namespace gtry::hlim {

class Node_External : public Node<Node_External>
{
    public:
        inline const std::string &getLibraryName() const { return m_libraryName; }
        inline const std::string &getPackageName() const { return m_packageName; }
        inline bool isEntity() const { return m_isEntity; }
        inline const std::map<std::string, std::string> &getGenericParameters() const { return m_genericParameters; }
        inline const std::vector<std::string> &getClockNames() const { return m_clockNames; }
        inline const std::vector<std::string> &getResetNames() const { return m_resetNames; }
    protected:
        bool m_isEntity = false;
        std::string m_libraryName;
        std::string m_packageName;
        std::map<std::string, std::string> m_genericParameters;
        std::vector<std::string> m_clockNames;
        std::vector<std::string> m_resetNames;
};

}
