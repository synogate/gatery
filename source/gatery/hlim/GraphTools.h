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

#include "../simulation/BitVectorState.h"

#include <span>

namespace gtry::hlim {

	class BaseNode;
	struct NodePort;
	class Circuit;
	class NodeGroup;
	class Node_Pin;
	class Clock;
	class Node_Register;
	class Node_RegSpawner;
	class Node_RegHint;
	class Subnet;

	sim::DefaultBitVectorState evaluateStatically(Circuit &circuit, hlim::NodePort output);

	Node_Pin* findInputPin(NodePort output);
	Node_Pin* findOutputPin(NodePort output);

	Clock* findFirstOutputClock(NodePort output);
	Clock* findFirstInputClock(NodePort input);

	std::vector<Node_Register*> findAllOutputRegisters(NodePort output);
	std::vector<Node_Register*> findAllInputRegisters(NodePort input);

	std::vector<Node_Register*> findRegistersAffectedByReset(Clock *clock);



	/**
	* @brief Finds and returns all Node_RegHints along with their distance (in Node_RegHint on the way) to Node_RegSpawner nodes.
	* @details Search is a forward-only dijkstra search from the Node_RegSpawner and confined to the given subnet.
	* @param spawners Where to start
	* @param subnet Confines the search
	* @return List of found Node_RegHints with their distance (as in number of Node_RegHints on the way) to the nearest Node_RegSpawner.
	*/
	std::vector<std::pair<size_t, Node_RegHint*>> getRegHintDistanceToSpawners(std::span<Node_RegSpawner*> spawners, const Subnet &subnet);

	/**
	 * @brief Get the number of registers between sourceOutput and destinationInput.
	 * @details Since multiple (potentially infinite) paths can exist between sourceOutput and destinationInput, the 
	 * path with the minimal number of registers is considered.
	 * @param sourceOutput Start of the path, given as an node output
	 * @param destinationInput End of the path, given as a node input
	 * @return Number of registers on the path or ~0ull if no path can be found
	 */
	size_t getMinRegsBetween(NodePort sourceOutput, NodePort destinationInput);

	/**
	 * @brief Get the number of register hints between sourceOutput and destinationInput.
	 * @details Since multiple (potentially infinite) paths can exist between sourceOutput and destinationInput, the 
	 * path with the minimal number of register hints is considered.
	 * @param sourceOutput Start of the path, given as an node output
	 * @param destinationInput End of the path, given as a node input
	 * @return Number of register hints on the path or ~0ull if no path can be found
	 */
	size_t getMinRegHintsBetween(NodePort sourceOutput, NodePort destinationInput);



}
