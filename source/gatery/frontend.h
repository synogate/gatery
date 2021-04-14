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
#pragma once

#include "pch.h"

#include "frontend/Bit.h"
#include "frontend/BitVector.h"
#include "frontend/BitWidth.h"
#include "frontend/Bundle.h"
#include "frontend/Clock.h"
#include "frontend/Comments.h"
#include "frontend/Compound.h"
#include "frontend/ConditionalScope.h"
#include "frontend/Constant.h"
#include "frontend/ConstructFrom.h"
#include "frontend/FrontendUnitTestSimulationFixture.h"
#include "frontend/FSM.h"
#include "frontend/Memory.h"
#include "frontend/Pack.h"
#include "frontend/Pin.h"
#include "frontend/PriorityConditional.h"
#include "frontend/Reg.h"
#include "frontend/Registers.h"
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
#include "frontend/Vector.h"
#if _MSC_FULL_VER > 192829336
#error The current msvc is buggy use MSVC 14.28.29333
#endif
