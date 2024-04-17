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
#pragma once

#include <gatery/scl/driver/MemoryMapEntry.h>

#include <gatery/frontend/BitWidth.h>
#include <gatery/utils/StableContainers.h>

#include <ostream>
#include <string>
#include <vector>


namespace gtry::scl
{

/**
 * @addtogroup gtry_scl_memorymaps
 * @{
 */

	struct AddressSpaceDescription {
		enum Flags
		{
			F_READ = 1,
			F_WRITE = 2,
		};

		/// Size of this field in the address space (in bits)
		BitWidth size = 0_b;
		/// Whether anything in this field (or the sub fields) can be read or written
		//size_t flags = F_READ | F_WRITE;
		/// Name of this address region
		std::string name;
		/// Short description of what this address space contains
		std::string descShort;
		/// Long description of what this address space contains
		std::string descLong;

		struct Child {
			/// Start of this field in the address space (in bits!)
			std::uint64_t offsetInBits = 0;
			std::shared_ptr<AddressSpaceDescription> desc;
		};

		/// Optional descriptions of sub-ranges in this address range
		std::vector<Child> children;

		/// Returns true, if this is just a helper instance for allowing arbitrary order construction of busses and information should be taken from its only child.
		bool isForwardingElement() const;

		/// If this->isForwardingElement(), returns the first non-forwarding child along the chain.
		AddressSpaceDescription *getNonForwardingElement();
		/// If this->isForwardingElement(), returns the first non-forwarding child along the chain.
		const AddressSpaceDescription *getNonForwardingElement() const;

		// AddressSpaceDescription() { }
	};

	using AddressSpaceDescriptionHandle = std::shared_ptr<AddressSpaceDescription>;

	void format(std::ostream &stream, const AddressSpaceDescription &desc, size_t indent = 0, std::uint64_t offset = 0ull);
	inline std::ostream &operator<<(std::ostream &stream, const AddressSpaceDescription &desc) { format(stream, desc); return stream; }

	namespace driver {
		struct MemoryMapEntry;
	}
	using FlatAddressSpaceDescription = std::vector<driver::MemoryMapEntry>;
	
	/// @note The resulting array is only valid as long as the AddressSpaceDescription does not go out of scope. To emphasize this, the function returns a copy of the handle.
	std::tuple<FlatAddressSpaceDescription, AddressSpaceDescriptionHandle> exportAddressSpaceDescription(AddressSpaceDescriptionHandle desc);
	void format(std::ostream &stream, std::string_view name, std::span<const driver::MemoryMapEntry> memoryMap);
	void writeGTKWaveFilterFile(std::ostream &stream, std::span<const driver::MemoryMapEntry> memoryMap);

/**@}*/
}

namespace gtry {
	inline scl::AddressSpaceDescriptionHandle constructFrom(const scl::AddressSpaceDescriptionHandle& val) { return std::make_shared<scl::AddressSpaceDescription>(); }
	void connectAddrDesc(scl::AddressSpaceDescriptionHandle &lhs, scl::AddressSpaceDescriptionHandle &rhs);
	inline void connect(scl::AddressSpaceDescriptionHandle &lhs, scl::AddressSpaceDescriptionHandle &rhs) { connectAddrDesc(lhs, rhs); }
}

