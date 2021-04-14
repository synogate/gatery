/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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

#include "BaseGrouping.h"

#include <ostream>


namespace hcl::vhdl {

class BasicBlock;


struct SequentialStatement
{
    enum Type {
        TYPE_ASSIGNMENT_EXPRESSION,
    };
    Type type;
    union {
        hlim::NodePort expressionProducer;
    } ref;
    size_t sortIdx = 0;

    inline bool operator<(const SequentialStatement &rhs) { return sortIdx < rhs.sortIdx; }
};


/**
 * @todo write docs
 */
class Process : public BaseGrouping
{
    public:
        Process(BasicBlock *parent);

        void buildFromNodes(const std::vector<hlim::BaseNode*> &nodes);
        virtual void extractSignals() override;
        virtual void writeVHDL(std::ostream &stream, unsigned indentation) = 0;
    protected:
        std::vector<hlim::BaseNode*> m_nodes;
};

class CombinatoryProcess : public Process
{
    public:
        CombinatoryProcess(BasicBlock *parent, const std::string &desiredName);

        virtual void allocateNames() override;

        virtual void writeVHDL(std::ostream &stream, unsigned indentation) override;
    protected:
        enum class Context {
            BOOL,
            STD_LOGIC,
            STD_LOGIC_VECTOR
        };

        void formatExpression(std::ostream &stream, std::ostream &comments, const hlim::NodePort &nodePort,
                                std::set<hlim::NodePort> &dependentInputs, Context context, bool forceUnfold = false);
};

struct RegisterConfig
{
    hlim::Clock *clock;
    bool hasResetSignal;
    inline bool operator<(const RegisterConfig &rhs) const { if (clock < rhs.clock) return true; if (clock > rhs.clock) return false; return hasResetSignal < rhs.hasResetSignal; }
};

class RegisterProcess : public Process
{
    public:
        RegisterProcess(BasicBlock *parent, const std::string &desiredName, const RegisterConfig &config);

        virtual void allocateNames() override;

        virtual void writeVHDL(std::ostream &stream, unsigned indentation) override;
    protected:
        RegisterConfig m_config;
};


}
