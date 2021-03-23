#pragma once

#include "../Node.h"

#include <vector>
#include <string>
#include <map>

namespace hcl::core::hlim {

class Node_External : public Node<Node_External>
{
    public:
        inline const std::string &getLibraryName() const { return m_libraryName; }
        inline const std::map<std::string, std::string> &getGenericParameters() const { return m_genericParameters; }
        inline const std::vector<std::string> &getClockNames() const { return m_clockNames; }
        inline const std::vector<std::string> &getResetNames() const { return m_resetNames; }
    protected:
        std::string m_libraryName;
        std::map<std::string, std::string> m_genericParameters;
        std::vector<std::string> m_clockNames;
        std::vector<std::string> m_resetNames;
};

}
