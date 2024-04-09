/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2024 Michael Offel, Andreas Ley

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

#include <gatery/scl_pch.h>

#include <gatery/scl/stream/utils.h>

GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::strm::PacketStream<gtry::BVec>)
GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::strm::RvPacketStream<gtry::BVec>)
GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::strm::VPacketStream<gtry::BVec>)
GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::strm::PacketStream<gtry::BVec, gtry::scl::strm::EmptyBits>)
GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::strm::RvPacketStream<gtry::BVec, gtry::scl::strm::EmptyBits>)
GTRY_INSTANTIATE_TEMPLATE_STREAM(gtry::scl::strm::VPacketStream<gtry::BVec, gtry::scl::strm::EmptyBits>)