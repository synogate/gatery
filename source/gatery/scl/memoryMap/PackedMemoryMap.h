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
	class PackedMemoryMap : public MemoryMap
	{
		public:
			struct RegisteredBaseSignal {
				BVec signal;
				Bit onWrite;
				CompoundMemberAnnotation *annotation;
			};

			struct PhysicalRegister {
				BVec signal;
				Bit onWrite;

				AddressSpaceDescription description;
			};

			struct Scope {
				std::string name;
				CompoundAnnotation *annotation;

				std::list<RegisteredBaseSignal> registeredSignals;
				gtry::Vector<PhysicalRegister> physicalRegisters;

				AddressSpaceDescription physicalDescription;

				std::list<Scope> subScopes;
			};

			PackedMemoryMap(std::string_view name, const CompoundAnnotation *anotation = nullptr);

			virtual void enterScope(std::string_view name, const CompoundAnnotation *anotation = nullptr) override;
			virtual void leaveScope() override;

			virtual SelectionHandle mapIn(ElementarySignal &value, std::string_view name, const CompoundMemberAnnotation *annotation = nullptr) override;
			virtual SelectionHandle mapOut(ElementarySignal &value, std::string_view name, const CompoundMemberAnnotation *annotation = nullptr) override;
			//virtual void reserve(BitWidth width, std::string_view name) override;

			virtual void pack(BitWidth registerWidth);
			const Scope &getTree() const { return m_scope; }
		protected:
			bool m_alreadyPacked = false;
			std::vector<std::string> m_scopeStack;
			Scope m_scope;
	};
}
