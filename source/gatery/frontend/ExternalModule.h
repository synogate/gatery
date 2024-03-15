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

	using GenericParameter = hlim::GenericParameter;
	using PinType = hlim::GenericParameter::BitFlavor;

	struct PinConfig
	{
		PinType type = PinType::STD_LOGIC;
		std::optional<Clock> clockOverride;
		/// Only valid for input bits. Returns true, if the module's outputs do not change when this input is deasserted (e.g. like a register's enable signal).
		bool isEnableSignal = false;
	};

	class ExternalModule
	{
	public:
		ExternalModule(std::string_view name, std::string_view library = {}, std::string_view package = {});
		ExternalModule(ExternalModule&&) = default;
		ExternalModule(const ExternalModule&) = delete;

		GenericParameter&	generic(std::string_view name);

		void clockIn(std::string_view name, std::string_view resetName = {});
		void clockIn(const Clock& clock, std::string_view name, std::string_view resetName = {});

		const Clock&		clockOut(std::string_view name, std::optional<std::string_view> resetName = {}, ClockConfig cfg = {});
		const Clock&		clockOut(std::string_view name, BitWidth W, size_t index, std::optional<std::string_view> resetName = {}, ClockConfig cfg = {});
		const Clock&		clockOut(const Clock& parentClock, std::string_view name, std::optional<std::string_view> resetName = {}, ClockConfig cfg = {});
		const Clock&		clockOut(const Clock& parentClock, std::string_view name, BitWidth W, size_t index, std::optional<std::string_view> resetName = {}, ClockConfig cfg = {});

		BVec&				in(std::string_view name, BitWidth W, PinConfig cfg = {});
		Bit&				in(std::string_view name, PinConfig cfg = {});
		const BVec&			in(std::string_view name, BitWidth W, PinConfig cfg = {}) const;
		const Bit&			in(std::string_view name, PinConfig cfg = {}) const;
		BVec				out(std::string_view name, BitWidth W, PinConfig cfg = {});
		Bit					out(std::string_view name, PinConfig cfg = {});

		void inoutPin(std::string_view portName, std::string_view pinName, BitWidth W, PinConfig cfg = {});
		void inoutPin(std::string_view portName, std::string_view pinName, PinConfig cfg = {});

		void isEntity(bool b) { m_node.isEntity(b); }
		void requiresComponentDeclaration(bool b) { m_node.requiresComponentDeclaration(b); }

		void hasSideEffects(bool b) { m_node.m_hasSideEffects = b; }
		bool hasSideEffects() const { return m_node.hasSideEffects(); }

	protected:
		const Clock& addClockOut(Clock clock, Bit clockSignal, std::optional<Bit> resetSignal);
		hlim::Node_External& node() { return m_node; }

	private:
		// todo: Node_External should have a interface which makes this class obsolete
		class Node_External_Exposed : public hlim::Node_External
		{
		public:
			using Node_External::resizeInputs;
			using Node_External::resizeOutputs;
			//using Node_External::declOutputBitVector;

			virtual bool hasSideEffects() const override { return m_hasSideEffects || hlim::Node_External::hasSideEffects(); }
			bool m_hasSideEffects = false;

			void isEntity(bool b) { m_isEntity = b; }
			void requiresComponentDeclaration(bool b) { m_requiresComponentDeclaration = b; }
			auto& generics() { return m_genericParameters; }
			auto& ins() { return m_inputPorts; }
			auto& outs() { return m_outputPorts; }
			//auto& clocks() { return m_clocks; }
			auto& clockNames() { return m_clockNames; }
			auto& resetNames() { return m_resetNames; }
			void library(std::string_view name) { m_libraryName = std::string{ name }; }
			void name(std::string_view name) { m_name = std::string{ name }; }
			void package(std::string_view name) { m_packageName = std::string{ name }; m_requiresNoFullInstantiationPath = true; m_isEntity = false; }

			std::unique_ptr<hlim::BaseNode> cloneUnconnected() const override;
			void copyBaseToClone(hlim::BaseNode *copy) const override;

			hlim::OutputClockRelation getOutputClockRelation(size_t output) const override;
			bool checkValidInputClocks(std::span<hlim::SignalClockDomain> inputClocks) const override;

			std::vector<hlim::Clock*> m_inClock;
			std::vector<hlim::OutputClockRelation> m_outClockRelations;

			void declareLastIOPairBidir() { declBidirPort(m_inputPorts.size()-1, m_outputPorts.size()-1); }

			using Node_External::declareInputIsEnable;
		};

		Node_External_Exposed& m_node;
		std::deque<Clock> m_outClock;
		std::deque<std::variant<Bit, BVec>> m_in;
	};

	/// @}
}

