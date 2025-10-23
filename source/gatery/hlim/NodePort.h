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

#include "NodePtr.h"

#include <limits>
#include <compare>
#include <stddef.h>

namespace gtry::hlim 
{
	class BaseNode;
	class NodeIO;
	class Circuit;
	struct ConnectionType;

	constexpr size_t INV_PORT = std::numeric_limits<std::size_t>::max();

	struct NodePort {
		BaseNode *node = nullptr;
		size_t port = INV_PORT;

		auto operator <=> (const NodePort&) const = default;
	};

	inline NodePort p0(BaseNode *node) { return {.node = node, .port = 0ull}; }

	struct RefCtdNodePort {
		NodePtr<BaseNode> node;
		size_t port = INV_PORT;

		RefCtdNodePort() = default;
		RefCtdNodePort(const NodePort &np);
		RefCtdNodePort& operator=(const NodePort &np);

		operator NodePort () const { return NodePort{.node = node.get(), .port = port}; }

		auto operator <=> (const RefCtdNodePort&) const = default;
	};

	bool outputIsBVec(const NodePort &output);
	bool outputIsBool(const NodePort &output);
	bool outputIsDependency(const NodePort &output);

	size_t getOutputWidth(const NodePort &output);
	const ConnectionType &getOutputConnectionType(const NodePort &output);

	struct In {};
	struct Out {};

	template<typename DirT>
	struct NodePortBit
	{
		const BaseNode* node = nullptr;
		size_t port = 0;
		size_t bit = 0;

		auto operator <=> (const NodePortBit&) const = default;
	};

	template<typename DirT>
	NodePort nodePort(const NodePortBit<DirT>& npb) { return NodePort{ npb.node, npb.port }; }

	template<typename DirT>
	NodePortBit<DirT> nodePortBit(const NodePort& np, size_t bit) { return NodePortBit<DirT>{ np.node, np.port, bit }; }
}

namespace std
{
	template<typename DirT>
	struct hash<gtry::hlim::NodePortBit<DirT>>
	{
		size_t operator ()(const gtry::hlim::NodePortBit<DirT>& port) const {
			return 
				hash<const gtry::hlim::BaseNode*>{}(port.node) ^
				hash<size_t>{}(port.port | (port.bit << 16));
		}
	};
}
