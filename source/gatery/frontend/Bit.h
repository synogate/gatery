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

#include "Signal.h"
#include "BitVectorSlice.h"

#include <gatery/hlim/NodePtr.h>
#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Traits.h>

#include <vector>
#include <optional>


namespace gtry {

namespace hlim {
	class Node_Signal;
	class SignalGroup;
}

/**
 * @addtogroup gtry_signals
 * @{
 */
 
	class Bit;
	struct construct_from_t;


	class BitDefault {
		public:
			BitDefault(const Bit& rhs);
			template<typename T, typename = std::enable_if_t<std::is_same_v<T, char> || std::is_same_v<T, bool>>>
			BitDefault(T v) { assign(v); }

			hlim::NodePort getNodePort() const { return m_nodePort; }
		protected:
			void assign(bool);
			void assign(char);

			hlim::RefCtdNodePort m_nodePort;
	};

	
	/**
	 * @brief Single bit signal
	 * @details An on-chip signal that represents a single bit. This bit can also be an alias of a single bit inside a bit vector type signal such that writing to this
	 * bit affects the bit vector. This is needed for handling the case of writing to an indexed bit of a bitvector, e.g. "bit_vector[i] = ...;".
	 */
	class Bit : public ElementarySignal
	{
	public:
		using isBitSignal = void;
		
		Bit();
		Bit(const Bit& rhs);
		Bit(const Bit& rhs, construct_from_t&&);
		Bit(Bit&& rhs);
		Bit(const BitDefault &defaultValue);

		/// For internal use by bit vector types only
		Bit(const SignalReadPort& port, std::optional<bool> resetValue = std::nullopt);
		Bit(hlim::Node_Signal* node, const std::shared_ptr<BitVectorSlice>& slice, size_t initialScopeId); // alias Bit

		/// Constructs a bit signal from a constant passed as a bit literal (char or bool).
		template<BitLiteral T>
		Bit(T v) {
			createNode();
			assign(v);
		}
	
		/// Signal assignment, preserves potential reset values.
		Bit& operator=(const Bit& rhs) { m_resetValue = rhs.m_resetValue; assign(rhs.readPort());  return *this; }
		Bit& operator=(Bit&& rhs);
		Bit& operator=(const BitDefault &defaultValue);

		/// Assigns (potentially conditionally) a constant passed as a bit literal (char or bool).
		template<BitLiteral T>
		Bit& operator=(T rhs) { assign(rhs); return *this; }

		/// Defines an alternative signal source that in the export should be used to drive all following logic.
		void exportOverride(const Bit& exportOverride);
		void simulationOverride(const Bit& simulationOverride);

		/// Always returns 1_b.
		BitWidth width() const final;
		/// Always returns a 1-wide bool-type.
		hlim::ConnectionType connType() const final;
		SignalReadPort readPort() const final;
		SignalReadPort outPort() const final;
		std::string_view getName() const final;
		void setName(std::string name) override;
		void setName(std::string name) const override;
		void addToSignalGroup(hlim::SignalGroup *signalGroup);

		/// Defines a reset value for this signal. All register created on this signal without an explicit reset value will use this reset value.
		void resetValue(bool v);
		/// Defines a reset value for this signal. All register created on this signal without an explicit reset value will use this reset value.
		void resetValue(char v);

		void resetValueRemove();
		/// Returns the optional reset value for this signal.
		/// @see resetValue(char v)
		std::optional<bool> resetValue() const { return m_resetValue; }

		hlim::Node_Signal* node() { return m_node; }
		const hlim::Node_Signal* node() const { return m_node; }

		virtual BVec toBVec() const override;
		virtual void fromBVec(const BVec &bvec) override;

	protected:
		void createNode();
		void assign(bool);
		void assign(char);
		virtual void assign(SignalReadPort in, bool ignoreConditions = false) override;

		bool valid() const final; // hide method since Bit is always valid
		SignalReadPort rawDriver() const;

	private:
		/// Signal node in the graph whose input represents this signal. It can be an alias of a bitvector, in which case m_offset or m_offsetDynamic identify the bit within.
		hlim::NodePtr<hlim::Node_Signal> m_node;

		std::shared_ptr<BitVectorSlice> m_slice;

		/// Optional reset value for this signal.
		/// @see resetValue(char v)
		std::optional<bool> m_resetValue;
	};

	// overload to implement reset value override

	struct RegisterSettings;
	/// Inserts a register between the input Bit and the returned bit, using the Bit's reset value for the register if available.
	Bit reg(const Bit& val, const RegisterSettings& settings);
	/// Inserts a register between the input Bit and the returned bit, using the Bit's reset value for the register if available.
	Bit reg(const Bit& val);

	class PipeBalanceGroup;
	/// Places a hint between the input and returned signals that a register should be retimed to this location.
	Bit pipestage(const Bit& signal);
	/// Adds the Bit to a PipeBalanceGroup, a group of signals that can receive pipeline registers but must remain balanced, but uses the Bit's reset value for the register if available.
	Bit pipeinput(const Bit& signal, PipeBalanceGroup& group);

	/// Adds a retiming blocker such that forward register retiming will not attempt to pull a register from this signal.
	Bit retimingBlocker(const Bit& signal);

	/// Overload of default final to account for reset value
	Bit final(const Bit& signal);

/**@}*/
}
