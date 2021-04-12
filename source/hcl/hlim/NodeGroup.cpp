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
#include "NodeGroup.h"

#include "coreNodes/Node_Signal.h"

#include "../utils/Range.h"

#include <boost/format.hpp>

#include <map>
#include <vector>

namespace hcl::core::hlim {


NodeGroup::NodeGroup(GroupType groupType) : m_groupType(groupType)
{
}

NodeGroup::~NodeGroup()
{
    while (!m_nodes.empty())
        m_nodes.front()->moveToGroup(nullptr);
}

void NodeGroup::reccurInferInstanceNames()
{
    if (m_parent == nullptr) // root node
        m_instanceName = m_name+"_inst";

    std::map<std::string, std::vector<NodeGroup*>> sortedChildren;
    for (auto &c : m_children)
        sortedChildren[c->getName()].push_back(c.get());

    for (auto &pair : sortedChildren) {
        if (pair.second.size() == 1)
            pair.second.front()->setInstanceName(pair.first+"_inst");
        else
            for (auto i : utils::Range(pair.second.size()))
                pair.second[i]->setInstanceName((boost::format("%s_inst_%d") % pair.first % i).str());
    }
    for (auto &c : m_children)
        c->reccurInferInstanceNames();
}

NodeGroup *NodeGroup::addChildNodeGroup(GroupType groupType)
{
    m_children.push_back(std::make_unique<NodeGroup>(groupType));
    m_children.back()->m_parent = this;
    return m_children.back().get();
}

void NodeGroup::moveInto(NodeGroup *newParent)
{
    size_t parentIdx = ~0ull;
    for (auto i : utils::Range(m_parent->m_children.size()))
        if (m_parent->m_children[i].get() == this) {
            parentIdx = i;
            break;
        }

    newParent->m_children.push_back(std::move(m_parent->m_children[parentIdx]));
    m_parent->m_children[parentIdx] = std::move(m_parent->m_children.back());
    m_parent->m_children.pop_back();
}

bool NodeGroup::isChildOf(const NodeGroup *other) const
{
    const NodeGroup *parent = getParent();
    while (parent != nullptr) {
        if (parent == other)
            return true;
        parent = parent->getParent();
    }
    return false;
}

bool NodeGroup::isEmpty(bool recursive) const
{
    if (!m_nodes.empty()) return false;

    if (recursive)
        for (auto &c : m_children)
            if (!c->isEmpty(recursive))
                return false;

    return true;
}


}
