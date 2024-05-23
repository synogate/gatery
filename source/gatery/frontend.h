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

#include "pch.h"

#include "frontend/Area.h"
#include "frontend/Attributes.h"
#include "frontend/Bit.h"
#include "frontend/BitWidth.h"
#include "frontend/BVec.h"
#include "frontend/Chrono.h"
#include "frontend/UInt.h"
#include "frontend/SInt.h"
#include "frontend/Enum.h"
#include "frontend/BitWidth.h"
#include "frontend/Bundle.h"
#include "frontend/Clock.h"
#include "frontend/Comments.h"
#include "frontend/Compound.h"
#include "frontend/ConditionalScope.h"
#include "frontend/Constant.h"
#include "frontend/ConstructFrom.h"
#include "frontend/DesignScope.h"
#include "frontend/ExternalComponent.h"
#include "frontend/ExternalModule.h"
#include "frontend/Final.h"
//#include "frontend/FrontendUnitTestSimulationFixture.h"
#include "frontend/FSM.h"
#include "frontend/GraphTools.h"
#include "frontend/Memory.h"
#include "frontend/Overrides.h"
#include "frontend/Pack.h"
#include "frontend/Pin.h"
#include "frontend/Pipe.h"
#include "frontend/PipeBalanceGroup.h"
#include "frontend/PriorityConditional.h"
#include "frontend/Reg.h"
#include "frontend/RetimingBlocker.h"
#include "frontend/Reverse.h"
#include "frontend/Scope.h"
#include "frontend/Signal.h"
#include "frontend/SignalArithmeticOp.h"
#include "frontend/SignalBitshiftOp.h"
#include "frontend/SignalCompareOp.h"
#include "frontend/SignalDelay.h"
#include "frontend/SignalGenerator.h"
#include "frontend/SignalLogicOp.h"
#include "frontend/SignalMiscOp.h"
#include "frontend/SimSigHandle.h"
//#include "frontend/tech/TechnologyMappingPattern.h"
#include "frontend/tech/TechnologyCapabilities.h"
//#include "frontend/tech/TargetTechnology.h"
#include "frontend/trace.h"
#include "frontend/Vector.h"
#include "frontend/CompoundTemplateInstantiations.h"
#include "debug/DebugInterface.h"

namespace gtry {
	using DefaultPostprocessing = hlim::DefaultPostprocessing;
}

/** @defgroup gtry_frontend Gatery - Frontend
 * @ingroup gtry
 * @brief Everything needed for composing circuits.
 * 
 * The Gatery frontend encompasses all classes and functions necessary for building 
 * circuits. See @ref gtry_frontend_page
 */


/** @defgroup gtry_signals Signals
 * @ingroup gtry_frontend
 * @brief All base signals
 * 
 */

/**
 * @defgroup gtry_scopes Scopes
 * @ingroup gtry_frontend
 * @brief All scopes
 *
 *
 */


/**
 * @defgroup gtry_simProcs Simulation Processes
 * @ingroup gtry_frontend
 * @brief All frontend constructs for use in simulation processes
 *
 *
 */

