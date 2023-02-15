/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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

#include <gatery/frontend/BVec.h>
#include <gatery/frontend/tech/TechnologyMappingPattern.h>

namespace gtry::hlim {
	class Clock;
}

namespace gtry::scl::arch {

class BaseDDROutPattern : public TechnologyMappingPattern
{
	public:
		virtual ~BaseDDROutPattern() = default;

		bool scopedAttemptApply(hlim::NodeGroup *nodeGroup) const;
	protected:
		std::string_view m_patternName;

		struct ReplaceInfo {
			hlim::Clock *clock;
			BVec reset;
			BVec D[2];
			BVec O;
		};

		virtual bool performReplacement(hlim::NodeGroup *nodeGroup, ReplaceInfo &replacement) const = 0;

		struct ConstResetReplaceInfo : ReplaceInfo {
			std::optional<bool> reset;
		};

		bool splitByReset(hlim::NodeGroup *nodeGroup, ReplaceInfo &replacement) const;

		virtual void performConstResetReplacement(hlim::NodeGroup *nodeGroup, ConstResetReplaceInfo &replacement) const { }
};


}
