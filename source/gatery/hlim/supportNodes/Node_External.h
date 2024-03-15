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

#include "../Node.h"

#include <gatery/simulation/BitVectorState.h>

#include <gatery/utils/ClangSpaceshipOptional.h>

#include <vector>
#include <string>
#include <map>
#include <variant>
#include <limits>

namespace gtry::hlim {

class GenericParameter {
	public:
		enum class DecimalFlavor {
			INTEGER,
			NATURAL,
			POSITIVE,
		};
		enum class BitFlavor {
			BIT,
			STD_LOGIC,
			STD_ULOGIC,
			VL,
		};
		//enum class BitVectorFlavor {
		//	BIT_VECTOR,
		//	STD_LOGIC_VECTOR
		//};

		GenericParameter &operator=(int v) { m_storage = v; m_flavor = DecimalFlavor::INTEGER; return *this; }
#ifdef __APPLE__
		GenericParameter &operator=(std::uint64_t v) { HCL_DESIGNCHECK_HINT(v <= std::numeric_limits<int>::max(), "Value too large!"); m_storage = (int) v; m_flavor = DecimalFlavor::NATURAL; return *this; }
#endif
		GenericParameter &operator=(size_t v) { HCL_DESIGNCHECK_HINT(v <= std::numeric_limits<int>::max(), "Value too large!"); m_storage = (int) v; m_flavor = DecimalFlavor::NATURAL; return *this; }
		GenericParameter &operator=(double v) { m_storage = v; return *this; }
		GenericParameter &operator=(std::string v) { m_storage = std::move(v); return *this; }
		GenericParameter &operator=(sim::DefaultBitVectorState v) { m_storage = std::move(v); m_flavor = BitFlavor::STD_LOGIC; return *this; }

		// too ambiguous
		// GenericParameter &operator=(bool v) { m_storage = v; }
		// GenericParameter &operator=(char v) { m_storage = v; }

		GenericParameter &setBoolean(bool v) { m_storage = v; return *this; }
		GenericParameter &setBit(char v, BitFlavor flavor = BitFlavor::BIT) { m_storage = v; m_flavor = flavor; return *this;}
		GenericParameter &setBitVector(size_t size, size_t value, BitFlavor flavor = BitFlavor::STD_LOGIC) { 
			m_storage = sim::createDefaultBitVectorState(size, value); 
			m_flavor = flavor; 
			return *this;
		}
		GenericParameter &setBitVector(sim::DefaultBitVectorState vec, BitFlavor flavor = BitFlavor::STD_LOGIC) { 
			m_storage = std::move(vec); 
			m_flavor = flavor; 
			return *this;
		}

		bool isDecimal() const { return std::holds_alternative<int>(m_storage); }
		bool isReal() const { return std::holds_alternative<double>(m_storage); }
		bool isString() const { return std::holds_alternative<std::string>(m_storage); }
		bool isBoolean() const { return std::holds_alternative<bool>(m_storage); }
		bool isBit() const { return std::holds_alternative<char>(m_storage); }
		bool isBitVector() const { return std::holds_alternative<sim::DefaultBitVectorState>(m_storage); }

		DecimalFlavor decimalFlavor() const { return std::get<DecimalFlavor>(m_flavor); }
		BitFlavor bitFlavor() const { return std::get<BitFlavor>(m_flavor); }
		//BitVectorFlavor bitVectorFlavor() const { return std::get<BitVectorFlavor>(m_flavor); }

		int decimal() const { return std::get<int>(m_storage); }
		double real() const { return std::get<double>(m_storage); }
		const std::string &string() const { return std::get<std::string>(m_storage); }
		bool boolean() const { return std::get<bool>(m_storage); }
		char bit() const { return std::get<char>(m_storage); }
		const sim::DefaultBitVectorState &bitVector() const { return std::get<sim::DefaultBitVectorState>(m_storage); }

		template<typename T>
		bool operator==(const T &rhs) const {
			if (!std::holds_alternative<T>(m_storage)) return false;
			return std::get<T>(m_storage) == rhs;
		}

		bool operator==(const char *rhs) const {
			if (!std::holds_alternative<std::string>(m_storage)) return false;
			return std::get<std::string>(m_storage) == rhs;
		}


		template<typename T>
		bool operator!=(const T &rhs) const { return !operator==(rhs); }
	protected:
		std::variant<DecimalFlavor, BitFlavor> m_flavor;
		std::variant<int, double, bool, char, std::string, sim::DefaultBitVectorState> m_storage;
};

class Node_External : public Node<Node_External>
{
	public:
		using BitFlavor = GenericParameter::BitFlavor;
		//using BitVectorFlavor = GenericParameter::BitVectorFlavor;
		
		struct Port {
			std::string name;
			std::string componentWidth;
			size_t instanceWidth;
			bool isVector;
			BitFlavor flavor;
			std::optional<size_t> bidirPartner;

			auto operator<=>(const Port &rhs) const = default;
		};

		inline const std::string &getLibraryName() const { return m_libraryName; }
		inline const std::string &getPackageName() const { return m_packageName; }
		inline bool isEntity() const { return m_isEntity; }
		inline bool requiresComponentDeclaration() const { return m_requiresComponentDeclaration; }
		inline bool requiresNoFullInstantiationPath() const { return m_requiresNoFullInstantiationPath; }
		inline const std::map<std::string, GenericParameter> &getGenericParameters() const { return m_genericParameters; }
		inline const std::vector<std::string> &getClockNames() const { return m_clockNames; }
		inline const std::vector<std::string> &getResetNames() const { return m_resetNames; }
		inline const std::vector<Port> &getInputPorts() const { return m_inputPorts; }
		inline const std::vector<Port> &getOutputPorts() const { return m_outputPorts; }

		virtual std::vector<std::string> getSupportFiles() const { return {}; }
		virtual void setupSupportFile(size_t idx, const std::string &filename, std::ostream &stream) { }


		void resizeIOPorts(size_t numInputs, size_t numOutputs);

		void declOutputBitVector(size_t idx, std::string name, size_t width, std::string componentWidth = {}, BitFlavor flavor = BitFlavor::STD_LOGIC);
		void declOutputBit(size_t idx, std::string name, BitFlavor flavor = BitFlavor::STD_LOGIC);

		void declInputBitVector(size_t idx, std::string name, size_t width, std::string componentWidth = {}, BitFlavor flavor = BitFlavor::STD_LOGIC);
		void declInputBit(size_t idx, std::string name, BitFlavor flavor = BitFlavor::STD_LOGIC);

		void declBidirPort(size_t inputIdx, size_t outputIdx);
		
		void changeInputWidth(size_t idx, size_t width);
		void changeOutputWidth(size_t idx, size_t width);

		virtual std::string getTypeName() const override { return m_name; }
		virtual void assertValidity() const override {}
		virtual std::string getInputName(size_t idx) const override;
		virtual std::string getOutputName(size_t idx) const override;

		virtual std::string attemptInferOutputName(size_t outputPort) const override;

		bool inputIsBidir(size_t idx) const { return (bool)m_inputPorts[idx].bidirPartner; }
		bool outputIsBidir(size_t idx) const { return (bool)m_outputPorts[idx].bidirPartner; }

		virtual bool inputIsEnable(size_t inputPort) const override;
	protected:
		bool m_isEntity = false;
		bool m_requiresComponentDeclaration = false;
		bool m_requiresNoFullInstantiationPath = false;
		std::string m_libraryName;
		std::string m_packageName;
		std::map<std::string, GenericParameter> m_genericParameters;
		std::vector<std::string> m_clockNames;
		std::vector<std::string> m_resetNames;
		std::vector<Port> m_inputPorts;
		std::vector<Port> m_outputPorts;
		std::set<size_t> m_inputIsEnable;

		virtual void resizeInputs(size_t num) override;
		virtual void resizeOutputs(size_t num) override;

		virtual void copyBaseToClone(BaseNode *copy) const override;

		void declareInputIsEnable(size_t inputPort);
};

}
