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

#include "NodePort.h"
#include "ConnectionType.h"
#include "GraphExploration.h"

#include <vector>

namespace gtry::hlim {

class BaseNode;
class NodeIO;
class Circuit;

class NodeIO
{
	public:
		enum OutputType {
			OUTPUT_IMMEDIATE,
			OUTPUT_LATCHED,
			// latched and immediate
			OUTPUT_CONSTANT
		};

		virtual ~NodeIO();

		inline size_t getNumInputPorts() const { return m_inputPorts.size(); }
		inline size_t getNumOutputPorts() const { return m_outputPorts.size(); }

		ConnectionType getDriverConnType(size_t inputPort) const;
		NodePort getDriver(size_t inputPort) const;
		NodePort getNonSignalDriver(size_t inputPort) const;

		const std::vector<NodePort> &getDirectlyDriven(size_t outputPort) const;

		ExplorationFwdDepthFirst exploreOutput(size_t port) { return ExplorationFwdDepthFirst({.node=(BaseNode*)this, .port = port}); }
		ExplorationBwdDepthFirst exploreInput(size_t port) { return ExplorationBwdDepthFirst({.node=(BaseNode*)this, .port = port}); }

		inline const ConnectionType &getOutputConnectionType(size_t outputPort) const { return m_outputPorts[outputPort].connectionType; }
		inline OutputType getOutputType(size_t outputPort) const { return m_outputPorts[outputPort].outputType; }

		virtual void bypassOutputToInput(size_t outputPort, size_t inputPort);

		inline void rewireInput(size_t inputPort, const NodePort &output) { connectInput(inputPort, output); }
	protected:
		void setOutputConnectionType(size_t outputPort, const ConnectionType &connectionType);
		void setOutputType(size_t outputPort, OutputType outputType);

		void connectInput(size_t inputPort, const NodePort &output);
		void disconnectInput(size_t inputPort);

		virtual void resizeInputs(size_t num);
		virtual void resizeOutputs(size_t num);
	private:
		struct OutputPort {
			ConnectionType connectionType;
			OutputType outputType = OUTPUT_IMMEDIATE;
			std::vector<NodePort> connections;
		};

		std::vector<NodePort> m_inputPorts;
		std::vector<OutputPort> m_outputPorts;

		friend class Circuit;
};


}
