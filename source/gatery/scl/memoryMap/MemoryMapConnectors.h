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

#include <boost/hana/fwd/define_struct.hpp>

namespace gtry::scl
{


/**
 * @addtogroup gtry_scl_memorymaps
 * @{
 */

	/// Type trait for providing custom handlers for registration in a @ref gtry::scl::MemoryMap for select types (i.e. streams, fifos, memories, ...)
	/// @see gtry_scl_memorymaps_page
	template<typename T>
	struct CustomMemoryMapHandler {
	};

	class MemoryMapRegistrationVisitor;

	/// Checks if a type has a custom handler for registration in a @ref gtry::scl::MemoryMap.
	template<typename T>
	concept HasCustomMemoryMapHandler = requires(MemoryMapRegistrationVisitor &v, T &t, bool b, 
										std::string_view n, const CompoundMemberAnnotation *a) { 
		CustomMemoryMapHandler<T>::memoryMap(v, t, b, n, a); 
	};

	/// Visitor class for @ref gtry::reccurseCompoundMembers to implement the recursive member registration for @ref gtry::scl::MemoryMap.
	class MemoryMapRegistrationVisitor
	{
		public:
			MemoryMapRegistrationVisitor(MemoryMap &memoryMap) : memoryMap(memoryMap) { }

			template<typename T>
			bool enterPackStruct(const T& member) {
				memberCounter.push_back(0);
				annotationStack.push_back(getAnnotation<T>());
				memoryMap.enterScope(lastName, annotationStack.back());
				return !HasCustomMemoryMapHandler<T>;
			}

			template<typename T>
			bool enterPackContainer(const T& member) {
				memberCounter.push_back(0);
				annotationStack.push_back(getAnnotation<T>());
				memoryMap.enterScope(lastName, getAnnotation<T>());
				return !HasCustomMemoryMapHandler<T>;
			}

			void reverse() { isReverse = !isReverse; }
			void leavePack() { memoryMap.leaveScope(); annotationStack.pop_back(); memberCounter.pop_back(); }

			template<typename T>
			void enter(const T &t, std::string_view name) { lastName = name; }
			void leave() { }

			template<typename T>
			void operator () (T& member) {
				const CompoundMemberAnnotation *memberAnnotation = nullptr;
				if (annotationStack.back() != nullptr) {
					HCL_DESIGNCHECK_HINT(memberCounter.back() < annotationStack.back()->memberDesc.size(), "A struct that is being registered in a memory map has an annotation/description of the members that does not match the actual number of members in the struct!");
					memberAnnotation = &annotationStack.back()->memberDesc[memberCounter.back()];
				}

				if constexpr (HasCustomMemoryMapHandler<T>)
					CustomMemoryMapHandler<T>::memoryMap(*this, member, isReverse, lastName, memberAnnotation);
				else
					if constexpr (std::derived_from<T, ElementarySignal>) {
						if (isReverse)
							memoryMap.readable(member, lastName, memberAnnotation);
						else
						{
							HCL_DESIGNCHECK_HINT(!std::is_const_v<T>, "cannot mapIn const signal");
							if constexpr (!std::is_const_v<T>)
								selectionHandle.joinWith(memoryMap.writeable(member, lastName, memberAnnotation));
						}
					}
				memberCounter.back()++;
			}

			MemoryMap::SelectionHandle selectionHandle;
			MemoryMap &memoryMap;
		protected:
			bool isReverse = false;
			std::string lastName;
			std::vector<const CompoundAnnotation *> annotationStack = { nullptr };
			std::vector<size_t> memberCounter = { 0 };
	};

	/**
	 * @brief Register a signal in a memory map as writeable from the bus master.
	 * @details If the signal is a compound, the registration proceeds recursively
	 * through the compound and registers each member individually.
	 * Much like for @ref gtry::pinIn, encountering a reverse signal (see @ref gtry::Reverse)
	 * flips the behaviour of the registration code and makes the signal readable by the bus master.
	 * Some types may have custom registration behaviors defined through @ref gtry::scl::CustomMemoryMapHandler.
	 * @param map The memory map to register in
	 * @param compound The signal or compound to register
	 * @param prefix The name under which to register in the memory map.
	 * @return MemoryMap::SelectionHandle A handle that allows querying for individual members of the compound, whether they are being written by the bus master.
	 * @see gtry_scl_memorymaps_page
	 */
	MemoryMap::SelectionHandle mapIn(MemoryMap &map, auto&& compound, std::string prefix)
	{
		MemoryMapRegistrationVisitor v(map);
		reccurseCompoundMembers(compound, v, prefix);
		return v.selectionHandle;
	}

	/**
	 * @brief Register a signal in a memory map as readable from the bus master.
	 * @details If the signal is a compound, the registration proceeds recursively
	 * through the compound and registers each member individually.
	 * Much like for @ref gtry::pinOut, encountering a reverse signal (see @ref gtry::Reverse)
	 * flips the behaviour of the registration code and makes the signal writeable by the bus master.
	 * Some types may have custom registration behaviors defined through @ref gtry::scl::CustomMemoryMapHandler.
	 * @param map The memory map to register in
	 * @param compound The signal or compound to register
	 * @param prefix The name under which to register in the memory map.
	 * @return MemoryMap::SelectionHandle A handle that allows querying for individual members of the compound, whether they are being written by the bus master.
	 * @see gtry_scl_memorymaps_page
	 */
	MemoryMap::SelectionHandle mapOut(MemoryMap &map, auto&& compound, std::string prefix)
	{
		MemoryMapRegistrationVisitor v(map);
		v.reverse();
		reccurseCompoundMembers(compound, v, prefix);
		return v.selectionHandle;
	}




	////////////////////////////////////////////////////

	// Todo: Move these to sensible locations


	template<typename T>
	struct CustomMemoryMapHandler<Memory<T>> {
		static void memoryMap(MemoryMapRegistrationVisitor &v, T &memory, bool isReverse, std::string_view name, const CompoundMemberAnnotation *annotation) {

			struct Cmd {
				BOOST_HANA_DEFINE_STRUCT(Cmd,
					(Bit, write),
					(BVec, address)
				);
			};

			Cmd cmd = { .address = memory.addressWidth() };
			Bit cmdTrigger = mapIn(v.memoryMap, cmd, "cmd").get(cmd.write);
			HCL_NAMED(cmdTrigger);
			HCL_NAMED(cmd);

			auto&& port = memory[cmd.address];

			T memContent = port.read();
			T stage = constructFrom(memContent);

			if (v.memoryMap.readEnabled())
				mapOut(v.memoryMap, stage, "stage");
			if (v.memoryMap.writeEnabled())
				mapIn(v.memoryMap, stage, "stage");

			IF(v.memoryMap.writeEnabled() & cmdTrigger & cmd.write)
				port = stage;

			if (v.memoryMap.readEnabled())
			{
				Bit dataAvailable;
				dataAvailable = reg(dataAvailable, '0');

				Bit readCmd = cmdTrigger & !cmd.write;
				IF (readCmd)
					dataAvailable = '0';
				T readData = memContent;
				for (auto i : utils::Range(memory.readLatencyHint())) {
					readCmd = reg(readCmd, '0');
					readData = reg(readData, { .allowRetimingBackwards = true });
				}
				HCL_NAMED(readCmd);
				HCL_NAMED(readData);

				IF(readCmd) {
					stage = readData;
					dataAvailable = '1';
				}

				HCL_NAMED(dataAvailable);
				mapOut(v.memoryMap, dataAvailable, "dataAvailable");
			}
		}
	};

	template<StreamSignal T>
	struct CustomMemoryMapHandler<T> {
		static void memoryMap(MemoryMapRegistrationVisitor &v, T &stream, bool isReverse, std::string_view name, const CompoundMemberAnnotation *annotation) {
			if (isReverse) {
				auto payload = constructFrom(*stream);
				connect(payload, *stream);
				mapOut(v.memoryMap, payload, "payload");

				if constexpr (T::template has<Ready>()) {
					Bit streamReady;
					if constexpr (T::template has<Valid>()) {
						Bit streamValid = valid(stream);
						if (v.memoryMap.readEnabled())
							mapOut(v.memoryMap, streamValid, "valid");

						// Make ready drop to low on transfer, and allow reading the ready flag to know when the payload register can be read.
						IF (streamValid)
							streamReady = '0';
						mapOut(v.memoryMap, streamReady, "ready");
					} else
						streamReady = '0';					

					mapIn(v.memoryMap, streamReady, "ready");
					ready(stream) = streamReady;
				}
			} else {

				auto payload = constructFrom(*stream);
				if (v.memoryMap.readEnabled())
					mapOut(v.memoryMap, payload, "payload");
				mapIn(v.memoryMap, payload, "payload");
				connect(*stream, payload);

				if constexpr (T::template has<Valid>()) {
					Bit streamValid;
					if constexpr (T::template has<Ready>()) {
						Bit streamReady = ready(stream);
						if (v.memoryMap.readEnabled())
							mapOut(v.memoryMap, streamReady, "ready"); // needed?

						// Make valid drop to low on transfer, and allow reading the valid flag to know when the next transfer can happen.
						IF (streamReady)
							streamValid = '0';
						mapOut(v.memoryMap, streamValid, "valid");
					} else
						streamValid = '0';
					mapIn(v.memoryMap, streamValid, "valid");
					valid(stream) = streamValid;
				}

				boost::hana::for_each(stream._sig, [&](auto& elem) {
					using MetaType = std::remove_cvref_t<decltype(elem)>;
					if constexpr (std::same_as<MetaType, Valid>) {
						// skip because it is handled separately
					} else
					if constexpr (std::same_as<MetaType, Ready>) {
						// skip because it is handled separately
					} else
						VisitCompound<MetaType>{}(elem, v);
				});
			}
		}
	};


/**@}*/

}
