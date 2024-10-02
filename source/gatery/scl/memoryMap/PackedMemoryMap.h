/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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

#include "MemoryMap.h"

namespace gtry::scl
{

/**
 * @addtogroup gtry_scl_memorymaps
 * @{
 */


	/**
	 * @brief Implementation for the automatic generation of memory mapped control registers.
	 * @details To register signals, use the @ref gtry::scl::mapIn and @ref gtry::scl::mapOut freestanding functions.
	 * @see gtry_scl_memorymaps_page
	 */
	class PackedMemoryMap : public MemoryMap
	{
		public:
			struct RegisteredBaseSignal {
				std::string name;
				std::optional<BVec> readSignal;
				std::optional<BVec> writeSignal;
				std::optional<Bit> onWrite;
				const CompoundMemberAnnotation *annotation;
			};

			struct PhysicalRegister {			
				std::optional<BVec> readSignal;
				std::optional<BVec> writeSignal;
				std::optional<Bit> onWrite;

				std::uint64_t offsetInBits = ~0ull;
				std::shared_ptr<AddressSpaceDescription> description;
			};

			struct Scope {
				std::string name;
				const CompoundAnnotation *annotation;

				std::list<RegisteredBaseSignal> registeredSignals;
				std::list<PhysicalRegister> physicalRegisters; // todo: move out of Scope

				std::uint64_t offsetInBits = ~0ull;
				std::shared_ptr<AddressSpaceDescription> physicalDescription;

				std::list<Scope> subScopes;
			};

			PackedMemoryMap(std::string_view name, const CompoundAnnotation *annotation = nullptr);

			virtual void enterScope(std::string_view name, const CompoundAnnotation *annotation = nullptr) override;
			virtual void leaveScope() override;

			virtual void readable(const ElementarySignal &value, std::string_view name, const CompoundMemberAnnotation *annotation = nullptr) override;
			virtual SelectionHandle writeable(ElementarySignal &value, std::string_view name, const CompoundMemberAnnotation *annotation = nullptr) override;
			//virtual void reserve(BitWidth width, std::string_view name) override;

			/// @brief Performs the actual address space allocation for the given data bus width.
			/// @details Not to be called directly, but by bus master adaptors such as @ref gtry::scl::toTileLinkUL
			virtual void packRegisters(BitWidth registerWidth);

			/// Access for bus master adaptors
			const Scope &getTree() const { return m_scope; }
			Scope &getTree() { return m_scope; }

			RegisteredBaseSignal& findSignal(std::vector<std::string_view> path);
		protected:
			Area m_area;
			bool m_alreadyPacked = false;
			std::vector<Scope*> m_scopeStack;
			Scope m_scope;

			void packRegisters(BitWidth registerWidth, Scope &scope);

			RegisteredBaseSignal *findSignal(Scope &scope, std::string_view name);
			std::string listRegisteredSignals(Scope& scope, std::string prefix);
	};

	void pinSimu(PackedMemoryMap::Scope& mmap, std::string prefix = "mmap");
/**@}*/

}
