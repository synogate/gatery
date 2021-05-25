#pragma once
#include "Scope.h"

namespace gtry
{
	class Area
	{
	public:
		Area(std::string_view name);
		
		GroupScope enter() const;
	
	private:
		hlim::NodeGroup* m_nodeGroup;
	};
}
