/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
#include "Bit.h"
#include "BVec.h"
#include "Clock.h"

#include <gatery/hlim/supportNodes/Node_External.h>

namespace gtry
{
	/**
	 * @addtogroup gtry_frontend
	 * @{
	 */

	using hlim::GenericParameter;

	class ExternalModule
	{
	public:
		enum class PinType
		{
			bit, std_logic, std_ulogic
		};

		struct PinConfig
		{
			PinType type = PinType::std_logic;
		};

	public:
		ExternalModule(std::string_view name, std::string_view library = {});

		GenericParameter&	generic(std::string_view name);

		const Clock&		clock(std::string_view name, std::optional<std::string_view> resetName = {}, ClockConfig cfg = {});
		const Clock&		clock(const Clock& parentClock, std::string_view name, std::optional<std::string_view> resetName = {}, ClockConfig cfg = {});

		BVec&				in(std::string_view name, BitWidth W, PinConfig cfg = {});
		Bit&				in(std::string_view name, PinConfig cfg = {});
		const BVec&			in(std::string_view name, BitWidth W, PinConfig cfg = {}) const;
		const Bit&			in(std::string_view name, PinConfig cfg = {}) const;
		BVec				out(std::string_view name, BitWidth W, PinConfig cfg = {});
		Bit					out(std::string_view name, PinConfig cfg = {});

	protected:
		const Clock& addClock(Clock clock, std::string_view pinName, std::optional<std::string_view> resetPinName);
		static GenericParameter::BitFlavor translateBitType(PinType type);
		static GenericParameter::BitVectorFlavor translateBVecType(PinType type);

	private:
		// todo: Node_External should have a interface which makes this class obsolete
		class Node_External_Exposed : public hlim::Node_External
		{
		public:
			using Node_External::resizeInputs;
			using Node_External::resizeOutputs;
			//using Node_External::declOutputBitVector;

			auto& generics() { return m_genericParameters; }
			auto& ins() { return m_inputPorts; }
			auto& outs() { return m_outputPorts; }
			//auto& clocks() { return m_clocks; }
			//auto& clockNames() { return m_clockNames; }
			void library(std::string_view name) { m_libraryName = std::string{ name }; }
			void name(std::string_view name) { m_name = std::string{ name }; }
			size_t clockIndex(hlim::Clock* clock);

			std::unique_ptr<BaseNode> cloneUnconnected() const override;

			hlim::OutputClockRelation getOutputClockRelation(size_t output) const override;
			bool checkValidInputClocks(std::span<hlim::SignalClockDomain> inputClocks) const override;

			std::vector<hlim::Clock*> m_inClock;
			hlim::OutputClockRelation m_outClock;
		};

		Node_External_Exposed& m_node;
		std::deque<Clock> m_clock;
		std::deque<std::variant<Bit, BVec>> m_in;
	};

	/// @}
}

