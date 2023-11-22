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

#include "PackedMemoryMap.h"
#include "../stream/strm.h"
#include "../Fifo.h"

namespace gtry::scl
{

	template<typename T>
	struct CustomMemoryMapHandler {
	};

	class MemoryMapRegistrationVisitor;

	template<typename T>
	concept HasCustomMemoryMapHandler = requires(MemoryMapRegistrationVisitor &v, T &t, bool b, 
										std::string_view n, const CompoundMemberAnnotation *a) { 
		CustomMemoryMapHandler<T>::memoryMap(v, t, b, n, a); 
	};

	class MemoryMapRegistrationVisitor
	{
		public:
			MemoryMapRegistrationVisitor(MemoryMap *memoryMap) : memoryMap(memoryMap) { }

			template<typename T>
			bool enterPackStruct(const T& member) {
				memoryMap->enterScope(lastName, getAnnotation<T>());
				return !HasCustomMemoryMapHandler<T>;
			}

			template<typename T>
			bool enterPackContainer(const T& member) {
				memoryMap->enterScope(lastName, getAnnotation<T>());
				return !HasCustomMemoryMapHandler<T>;
			}

			void reverse() { isReverse = !isReverse; }
			void leavePack() { memoryMap->leaveScope(); }

			template<typename T>
			void enter(const T &t, std::string_view name) { lastName = name; }
			void leave() { }

			template<typename T>
			void operator () (T& member) {
				if constexpr (HasCustomMemoryMapHandler<T>)
					CustomMemoryMapHandler<T>::memoryMap(*this, member, isReverse);
				else
					if constexpr (std::derived_from<ElementarySignal, T>) {
						if (isReverse)
							selectionHandle.joinWith(memoryMap->mapOut(member, lastName, nullptr));
						else
							selectionHandle.joinWith(memoryMap->mapIn(member, lastName, nullptr));
					}
			}

			MemoryMap::SelectionHandle selectionHandle;
			MemoryMap *memoryMap = nullptr;
		protected:
			bool isReverse = false;

			MemoryMap::SelectionHandle selectionHandle;
			std::string lastName;
	};

	MemoryMap::SelectionHandle mapIn(MemoryMap &map, auto&& compound, std::string prefix)
	{
		MemoryMapRegistrationVisitor v(&map);
		reccurseCompoundMembers(compound, v, prefix);
		return v.selectionHandle;
	}

	MemoryMap::SelectionHandle mapOut(MemoryMap &map, auto&& compound, std::string prefix)
	{
		MemoryMapRegistrationVisitor v(&map);
		v.reverse();
		reccurseCompoundMembers(compound, v, prefix);
		return v.selectionHandle;
	}




	////////////////////////////////////////////////////

	// Todo: Move these to responsible locations

	template<typename T>
	struct CustomMemoryMapHandler<Memory<T>> {
		static void memoryMap(MemoryMapRegistrationVisitor &v, T &stream, bool isReverse, std::string_view name, const CompoundMemberAnnotation *annotation) {
			UInt cmdAddr = "32xX";
			Bit cmdTrigger = wo(cmdAddr, {
				.name = "cmd"
			});
			HCL_NAMED(cmdTrigger);
			HCL_NAMED(cmdAddr);

			auto&& port = mem[cmdAddr(0, mem.addressWidth())];

			T memContent = port.read();
			T stage = constructFrom(memContent);

			SigVis v{ this };
			VisitCompound<T>{}(stage, v);
			stage = reg(stage);

			IF(writeEnabled() & cmdTrigger & cmdAddr.msb() == '0')
				port = stage;

			if (readEnabled())
			{
				Bit readCmd = reg(cmdTrigger & cmdAddr.msb() == '1', '0');
				T readData = reg(memContent);
				HCL_NAMED(readCmd);
				HCL_NAMED(readData);

				IF(readCmd)
					stage = readData;
			}
		}
	};	

	template<StreamSignal T>
	struct CustomMemoryMapHandler {
		static void memoryMap(MemoryMapRegistrationVisitor &v, T &stream, bool isReverse, std::string_view name, const CompoundMemberAnnotation *annotation) {
			VisitCompound<T>{}(stream, v);
		}
	};

}
