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

#include <gatery/frontend/ExternalComponent.h>
#include <gatery/frontend/tech/TechnologyMappingPattern.h>

#include "../general/BaseDDROutPattern.h"

namespace gtry::scl::arch::xilinx {

class ODDR : public gtry::ExternalComponent
{
	public:

		enum Clocks {
			CLK_IN,
			CLK_COUNT
		};

		enum Inputs {
			IN_D1,
			IN_D2,
			IN_SET,
			IN_RESET,
			IN_CE,

			IN_COUNT
		};
		enum Outputs {
        	OUT_Q,

			OUT_COUNT
		};


		enum DDR_CLK_EDGE {
			OPPOSITE_EDGE,
			SAME_EDGE
		};

		enum SRTYPE {
			ASYNC,
			SYNC
		};

		ODDR();

		virtual void setInput(size_t input, const Bit &bit) override;

		void setResetType(SRTYPE srtype);
		void setEdgeMode(DDR_CLK_EDGE edgeMode);
		void setInitialOutputValue(bool value);

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
};




class ODDRPattern : public BaseDDROutPattern
{
	public:
		ODDRPattern() { m_patternName = "ODDR"; }
		virtual ~ODDRPattern() = default;

	protected:
		virtual bool performReplacement(hlim::NodeGroup *nodeGroup, ReplaceInfo &replacement) const override;
		virtual void performConstResetReplacement(hlim::NodeGroup *nodeGroup, ConstResetReplaceInfo &replacement) const override;
};


}
