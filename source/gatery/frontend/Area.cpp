#include "gatery/pch.h"
#include "Area.h"

gtry::Area::Area(std::string_view name)
{
    auto* parent = GroupScope::getCurrentNodeGroup();
    m_nodeGroup = parent->addChildNodeGroup(hlim::NodeGroup::GroupType::ENTITY);
    m_nodeGroup->recordStackTrace();
    m_nodeGroup->setName(std::string(name));
}

gtry::GroupScope gtry::Area::enter() const
{
    return { m_nodeGroup };
}
