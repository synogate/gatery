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

#include <functional>
#include <vector>
#include <map>
#include <span>

#include <gatery/utils/StableContainers.h>

namespace gtry::hlim {
	class BaseNode;
	class NodeGroup;
	class MemoryGroup;
}

namespace gtry::scl::arch::memtools {

bool memoryIsSingleClock(hlim::NodeGroup *group);

struct SplitMemoryGroup {
	hlim::MemoryGroup* subGroup;
	utils::StableMap<hlim::BaseNode*, hlim::BaseNode*> original2subGroup;
};

/**
 * @brief Split an exisiting memory (with retimed registers) along the depth dimension into multiple memories.
 * @details The old memory remains in place and connected. The new memories are created as sub-NodeGroups of the memory NodeGroup and are unconnected.
 * Copies any reset logic still connected to the Node_Memory node.
 */
std::vector<SplitMemoryGroup> createDepthSplitMemories(hlim::NodeGroup *group, const std::span<size_t> &splits);

void splitMemoryAlongDepthMux(hlim::NodeGroup *group, size_t log2SplitDepth, bool placeInputRegs, bool placeOutputRegs);


/**
 * @brief Split an exisiting memory (with retimed registers) along the width dimension into multiple memories.
 * @details The old memory remains in place and connected. The new memories are created as sub-NodeGroups of the memory NodeGroup and are unconnected.
 * Can not deal with reset logic still connected to the Node_Memory node. Resolve resets first.
 */
std::vector<SplitMemoryGroup> createWidthSplitMemories(hlim::NodeGroup *group, const std::span<size_t> &splits);
void splitMemoryAlongWidth(hlim::NodeGroup *group, size_t maxWidth);


}