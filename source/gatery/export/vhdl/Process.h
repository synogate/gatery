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

#include "BaseGrouping.h"
#include "../../hlim/Attributes.h"
#include "../../hlim/Clock.h"

#include <ostream>


namespace gtry::vhdl {

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

		void buildFromNodes(std::vector<hlim::BaseNode*> nodes);
		virtual void extractSignals() override;
		virtual void writeVHDL(std::ostream &stream, unsigned indentation) = 0;

		virtual std::string getInstanceName() override { return m_name; }
		inline const std::set<hlim::NodePort> &getNonVariableSignals() const { return m_nonVariableSignals; }
	protected:
		std::set<hlim::NodePort> m_nonVariableSignals;
		std::vector<hlim::BaseNode*> m_nodes;
};

class CombinatoryProcess : public Process
{
	public:
		CombinatoryProcess(BasicBlock *parent, const std::string &desiredName);

		virtual void allocateNames() override;

		virtual void writeVHDL(std::ostream &stream, unsigned indentation) override;
	protected:
		void formatExpression(std::ostream &stream, size_t indentation, std::ostream &comments, const hlim::NodePort &nodePort,
								std::set<hlim::NodePort> &dependentInputs, VHDLDataType context, bool forceUnfold = false);
};

struct RegisterConfig
{
	hlim::Clock *clock = nullptr;
	hlim::Clock *reset = nullptr;
	hlim::Clock::TriggerEvent triggerEvent = hlim::Clock::TriggerEvent::RISING;
	hlim::RegisterAttributes::ResetType resetType = hlim::RegisterAttributes::ResetType::NONE;
	bool resetHighActive = true;

	auto operator<=>(const RegisterConfig&) const = default;

	static RegisterConfig fromClock(hlim::Clock *c, bool hasResetValue);
};

class RegisterProcess : public Process
{
	public:
		RegisterProcess(BasicBlock *parent, const std::string &desiredName, const RegisterConfig &config);

		virtual void extractSignals() override;
		virtual void allocateNames() override;

		virtual void writeVHDL(std::ostream &stream, unsigned indentation) override;
	protected:
		RegisterConfig m_config;
};


}
