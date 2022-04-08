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

#include "Bit.h"
#include "BitWidth.h"
#include "Signal.h"
#include "Scope.h"
#include "DesignScope.h"

#include "Constant.h"

#include <gatery/utils/Traits.h>

#include <gatery/hlim/coreNodes/Node_Constant.h>
#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/coreNodes/Node_Rewire.h>

#include <gatery/utils/Exceptions.h>

#include <vector>
#include <algorithm>
#include <compare>
#include <optional>
#include <map>

namespace gtry {

	struct Selection {
		int start = 0;
		int width = 0;
		bool untilEndOfSource = false;

		static Selection All();
		static Selection From(int start);
		static Selection Range(int start, int end);
		static Selection RangeIncl(int start, int endIncl);

		static Selection Slice(size_t offset, size_t size);

		static Selection Symbol(int idx, BitWidth symbolWidth);
		static Selection Symbol(size_t idx, BitWidth symbolWidth) { return Symbol(int(idx), symbolWidth); }

		auto operator <=> (const Selection& rhs) const = default;
	};

	struct SymbolSelect
	{
		BitWidth symbolWidth;
		Selection operator [] (int idx) const { return Selection::Symbol(idx, symbolWidth); }
		Selection operator [] (size_t idx) const { return Selection::Symbol(int(idx), symbolWidth); }
	};


    class BaseBitVectorDefault {
        public:
            BaseBitVectorDefault(const BaseBitVector& rhs);
			BaseBitVectorDefault(std::int64_t value);
			BaseBitVectorDefault(std::uint64_t value);
			BaseBitVectorDefault(std::string_view);

            hlim::NodePort getNodePort() const { return m_nodePort; }
        protected:
            hlim::RefCtdNodePort m_nodePort;
    };

	class BaseBitVector : public ElementarySignal
	{
	public:
		struct Range {
			Range() = default;
			Range(const Range&) = default;
			Range(const Selection& s, const Range& r);

			auto operator <=> (const Range&) const = default;

			size_t bitOffset(size_t idx) const { HCL_ASSERT(idx < width); return offset + idx; }

			size_t width = 0;
			size_t offset = 0;
			bool subset = false;
		};

		using isBitVectorSignal = void;

		using iterator = std::vector<Bit>::iterator;
		using const_iterator = std::vector<Bit>::const_iterator;
		using reverse_iterator = std::vector<Bit>::reverse_iterator;
		using const_reverse_iterator = std::vector<Bit>::const_reverse_iterator;
		using value_type = Bit;

		~BaseBitVector();

		template<BitVectorDerived T>
		explicit operator T() const { if (m_node) return T(getReadPort()); else return T{}; }

		virtual void resize(size_t width) final;

		Bit& lsb() { return aliasLsb(); }
		const Bit& lsb() const { return aliasLsb(); }
		Bit& msb() { return aliasMsb(); }
		const Bit& msb() const { return aliasMsb(); }

		Bit& operator[](size_t idx) { return aliasVec()[idx]; }
		const Bit& operator[](size_t idx) const { return aliasVec()[idx]; }

		Bit& at(size_t idx) { return aliasVec().at(idx); }
		const Bit& at(size_t idx) const { return aliasVec().at(idx); }

		bool empty() const { return size() == 0; }
		size_t size() const { return width().value; }

		Bit& front() { return aliasVec().front(); }
		const Bit& front() const { return aliasVec().front(); }
		Bit& back() { return aliasVec().back(); }
		const Bit& back() const { return aliasVec().back(); }

		iterator begin() { return aliasVec().begin(); }
		iterator end() { return aliasVec().end(); }
		const_iterator begin() const { return aliasVec().begin(); }
		const_iterator end() const { return aliasVec().end(); }
		const_iterator cbegin() const { return aliasVec().cbegin(); }
		const_iterator cend() const { return aliasVec().cend(); }

		reverse_iterator rbegin() { return aliasVec().rbegin(); }
		reverse_iterator rend() { return aliasVec().rend(); }
		const_reverse_iterator rbegin() const { return aliasVec().rbegin(); }
		const_reverse_iterator rend() const { return aliasVec().rend(); }
		const_reverse_iterator crbegin() const { return aliasVec().crbegin(); }
		const_reverse_iterator crend() const { return aliasVec().crend(); }


        bool valid() const final { return m_node != nullptr; }
        hlim::Node_Signal* getNode() { return m_node; }

		// these methods are undefined for invalid signals (uninitialized)
		BitWidth width() const final { return BitWidth{ m_range.width }; }
		hlim::ConnectionType getConnType() const final;
		SignalReadPort getReadPort() const final;
		SignalReadPort getOutPort() const final;
		std::string_view getName() const final;
		void setName(std::string name) override;
		void addToSignalGroup(hlim::SignalGroup *signalGroup);

	protected:
		BaseBitVector() = default;
		BaseBitVector(const BaseBitVector& rhs) { if(rhs.m_node) assign(rhs.getReadPort()); }
		BaseBitVector(BaseBitVector&& rhs);
		BaseBitVector(const BaseBitVectorDefault &defaultValue);

		BaseBitVector(const SignalReadPort& port) { assign(port); }
		BaseBitVector(hlim::Node_Signal* node, Range range, Expansion expansionPolicy, size_t initialScopeId); // alias
		BaseBitVector(BitWidth width, Expansion expansionPolicy = Expansion::none);


		BaseBitVector& operator = (const BaseBitVector& rhs) { if (rhs.m_node) assign(rhs.getReadPort()); return *this; }
		BaseBitVector& operator = (BaseBitVector&& rhs);
		BaseBitVector& operator = (BitWidth width);
		BaseBitVector& operator = (const BaseBitVectorDefault &defaultValue);
	protected:
		void setExportOverride(const BaseBitVector& exportOverride);

		void assign(std::uint64_t value, Expansion policy);
		void assign(std::int64_t value, Expansion policy);
		void assign(std::string_view, Expansion policy);
		virtual void assign(SignalReadPort, bool ignoreConditions = false);

		void createNode(size_t width, Expansion policy);
		SignalReadPort getRawDriver() const;

		inline hlim::Node_Signal* getNode() const { return m_node; }
		inline const Range &getRange() const { return m_range; }
		inline const Expansion &getExpansionPolicy() const { return m_expansionPolicy; }
	private:
		hlim::Node_Signal* m_node = nullptr;
		Range m_range;
		Expansion m_expansionPolicy = Expansion::none;

		std::vector<Bit>& aliasVec() const;
		Bit& aliasMsb() const;
		Bit& aliasLsb() const;

		mutable std::vector<Bit> m_bitAlias;
		mutable std::optional<Bit> m_lsbAlias;
		mutable std::optional<Bit> m_msbAlias;

		mutable SignalReadPort m_readPort;
		mutable void* m_readPortDriver = nullptr;
	};

	inline BaseBitVector::iterator begin(BaseBitVector& bvec) { return bvec.begin(); }
	inline BaseBitVector::iterator end(BaseBitVector& bvec) { return bvec.end(); }
	inline BaseBitVector::const_iterator begin(const BaseBitVector& bvec) { return bvec.begin(); }
	inline BaseBitVector::const_iterator end(const BaseBitVector& bvec) { return bvec.end(); }

	template<typename FinalType, typename DefaultValueType>
	class SliceableBitVector : public BaseBitVector
	{
	public:
		using Range = BaseBitVector::Range;
		using OwnType = SliceableBitVector<FinalType, DefaultValueType>;


		SliceableBitVector() = default;
		SliceableBitVector(OwnType&& rhs) : BaseBitVector(std::move(rhs)) { }
		SliceableBitVector(const DefaultValueType &defaultValue) : BaseBitVector(defaultValue) { }

		SliceableBitVector(const SignalReadPort& port) : BaseBitVector(port) { }
		SliceableBitVector(hlim::Node_Signal* node, Range range, Expansion expansionPolicy, size_t initialScopeId) : BaseBitVector(node, range, expansionPolicy, initialScopeId) { } // alias
		SliceableBitVector(BitWidth width, Expansion expansionPolicy = Expansion::none) : BaseBitVector(width, expansionPolicy) { }

		SliceableBitVector(const OwnType &rhs) : BaseBitVector(rhs) { } // Necessary because otherwise deleted copy ctor takes precedence over templated ctor.

		FinalType& operator = (BitWidth width) { BaseBitVector::operator=(width); return (FinalType&)*this; }
		FinalType& operator = (const DefaultValueType &defaultValue) { BaseBitVector::operator=(defaultValue); return (FinalType&)*this; }

		void setExportOverride(const FinalType& exportOverride) { BaseBitVector::setExportOverride(exportOverride); }

		template<std::integral Int1, std::integral Int2>
		FinalType& operator()(Int1 offset, Int2 size) { return (*this)(Selection::Slice(int(offset), int(size))); }
		template<std::integral Int1, std::integral Int2>
		const FinalType& operator() (Int1 offset, Int2 size) const { return (*this)(Selection::Slice(int(offset), int(size))); }
		FinalType& operator() (size_t offset, BitWidth size) { return (*this)(Selection::Slice(int(offset), int(size.value))); }
		const FinalType& operator() (size_t offset, BitWidth size) const { return (*this)(Selection::Slice(int(offset), int(size.value))); }

		FinalType& operator()(const Selection& selection) { return aliasRange(Range(selection, getRange())); }
		const FinalType& operator() (const Selection& selection) const { return aliasRange(Range(selection, getRange())); }
	protected:
		inline FinalType& aliasRange(const Range& range) const {
        	auto [it, exists] = m_rangeAlias.try_emplace(range, getNode(), range, getExpansionPolicy(), m_initialScopeId);
        	return it->second;
		}
		mutable std::map<Range, FinalType> m_rangeAlias;
	};

	template<BitVectorSignal T>
	T extTo(const T& vec, BitWidth width) { HCL_DESIGNCHECK_HINT(width >= vec.width(), "extTo can not make a vector smaller"); return ext(vec, (width - vec.width()).value); }
	template<BitVectorSignal T>
	T extTo(const T& vec, BitWidth width, Expansion policy) { HCL_DESIGNCHECK_HINT(width >= vec.width(), "extTo can not make a vector smaller"); return ext(vec, (width - vec.width()).value, policy); }

	template<BitVectorLiteral T>
	auto extTo(const T& vec, BitWidth width) { return extTo<ValueToBaseSignal<T>>(vec, width); }
	template<BitVectorLiteral T>
	auto extTo(const T& vec, BitWidth width, Expansion policy) { return extTo<ValueToBaseSignal<T>>(vec, width, policy); }

	template<BitVectorValue T>
	inline auto zextTo(const T& vec, BitWidth width) { return ext(vec, width, Expansion::zero); }
	template<BitVectorValue T>
	inline auto oextTo(const T& vec, BitWidth width) { return ext(vec, width, Expansion::one); }
	template<BitVectorValue T>
	inline auto sextTo(const T& vec, BitWidth width) { return ext(vec, width, Expansion::sign); }



	template<BitVectorSignal T>
	T resizeTo(const T& vec, BitWidth width) { if (width >= vec.width()) return extTo(vec, width); else return vec(0, width); }

	template<BitVectorLiteral T>
	auto resizeTo(const T& vec, BitWidth width) { return extTo<ValueToBaseSignal<T>>(vec, width); }

}
