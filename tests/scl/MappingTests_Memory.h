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

#include <gatery/frontend/GHDLTestFixture.h>

#include <gatery/frontend.h>


struct Test_Histogram : public gtry::GHDLTestFixture
{
	size_t numBuckets = 32;
	gtry::BitWidth bucketWidth = gtry::BitWidth(8);
	size_t iterationFactor = 8;
	bool highLatencyExternal = false;
	bool twoCycleLatencyBRam = false;
	bool forceMemoryResetLogic = false;
	bool forceNoInitialization = false;
	bool forceNoEnable = false;
	void execute();
};

struct Test_MemoryCascade : public gtry::GHDLTestFixture
{
	size_t depth = 1 << 16;
	gtry::BitWidth elemSize = gtry::BitWidth(2);
	size_t numWrites = 1000;
	bool forceMemoryResetLogic = false;
	bool forceNoInitialization = false;
	void execute();
};

struct Test_SDP_DualClock : public gtry::GHDLTestFixture
{
	size_t depth = 1 << 16;
	gtry::BitWidth elemSize = gtry::BitWidth(2);
	size_t numWrites = 1000;
	bool forceMemoryResetLogic = false;
	bool forceNoInitialization = false;
	void execute();
};


struct Test_ReadEnable : public gtry::GHDLTestFixture
{
	size_t numElements = 32;
	gtry::BitWidth elementWidth = gtry::BitWidth(8);
	size_t iterationFactor = 8;
	bool highLatencyExternal = false;
	bool twoCycleLatencyBRam = false;
	void execute();
};

