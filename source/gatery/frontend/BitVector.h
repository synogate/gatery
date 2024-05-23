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

#include <gatery/utils/StableContainers.h>

#include "Bit.h"
#include "BitWidth.h"
#include "Signal.h"
#include "BitVectorSlice.h"

#include <gatery/utils/Traits.h>

#include <gatery/utils/Exceptions.h>

#include <vector>
#include <algorithm>
#include <compare>
#include <optional>
#include <map>
#include <variant>

namespace gtry {

	class UInt;

	class BaseBitVectorDefault {
		public:
			explicit BaseBitVectorDefault(const BaseBitVector& rhs);
			explicit BaseBitVectorDefault(std::int64_t value);
			explicit BaseBitVectorDefault(std::uint64_t value);
			explicit BaseBitVectorDefault(std::string_view);

			hlim::NodePort getNodePort() const { return m_nodePort; }
		protected:
			hlim::RefCtdNodePort m_nodePort;
	};

	class BaseBitVector : public ElementarySignal
	{
	public:
		using isBitVectorSignal = void;

		using iterator = std::vector<Bit>::iterator;
		using const_iterator = std::vector<Bit>::const_iterator;
		using reverse_iterator = std::vector<Bit>::reverse_iterator;
		using const_reverse_iterator = std::vector<Bit>::const_reverse_iterator;
		using value_type = Bit;

		BaseBitVector() = default;
		BaseBitVector(const BaseBitVector& rhs) { if (rhs.m_node) assign(rhs.readPort()); }
		BaseBitVector(BaseBitVector&& rhs);
		BaseBitVector(const BaseBitVectorDefault& defaultValue);

		BaseBitVector(const SignalReadPort& port) { assign(port); }
		BaseBitVector(hlim::Node_Signal* node, std::shared_ptr<BitVectorSlice> range, Expansion expansionPolicy, size_t initialScopeId); // alias
		BaseBitVector(BitWidth width, Expansion expansionPolicy = Expansion::none);

		template<BitVectorDerived T>
		explicit operator T() const { if (m_node) return T(readPort()); else return T{}; }

		void resize(size_t width);
		virtual void resetNode();

		Bit& lsb() { return aliasLsb(); }
		const Bit& lsb() const { return aliasLsb(); }
		Bit& msb() { return aliasMsb(); }
		const Bit& msb() const { return aliasMsb(); }

		Bit& operator[](int idx) { HCL_DESIGNCHECK(size() > 0); return aliasVec()[(size()+idx)%size()]; }
		Bit& operator[](size_t idx) { HCL_DESIGNCHECK(idx < size()); return aliasVec()[idx]; }
		const Bit& operator[](size_t idx) const { HCL_DESIGNCHECK(idx < size()); return aliasVec()[idx]; }

		Bit& operator[](const UInt &idx);
		const Bit& operator[](const UInt &idx) const;

		Bit& at(size_t idx) { return aliasVec().at(idx); }
		const Bit& at(size_t idx) const { return aliasVec().at(idx); }

		bool empty() const { return size() == 0; }

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
		hlim::Node_Signal* node() { return m_node; }

		// these methods are undefined for invalid signals (uninitialized)
		BitWidth width() const override final { return m_width; }
		hlim::ConnectionType connType() const override final;
		SignalReadPort readPort() const override final;
		SignalReadPort outPort() const override final;
		std::string_view getName() const override final;
		void setName(std::string name) override;
		void setName(std::string name) const override;
		void addToSignalGroup(hlim::SignalGroup *signalGroup);

	protected:
		BaseBitVector& operator = (const BaseBitVector& rhs) { if (rhs.m_node) assign(rhs.readPort()); return *this; }
		BaseBitVector& operator = (BaseBitVector&& rhs);
		BaseBitVector& operator = (BitWidth width);
		BaseBitVector& operator = (const BaseBitVectorDefault &defaultValue);

	protected:
		void exportOverride(const BaseBitVector& exportOverride);
		void simulationOverride(const BaseBitVector& simulationOverride);

		void assign(std::uint64_t value, Expansion policy);
		void assign(std::int64_t value, Expansion policy);
		void assign(std::string_view, Expansion policy);
		virtual void assign(SignalReadPort, bool ignoreConditions = false) override;

		void createNode(size_t width, Expansion policy);
		SignalReadPort rawDriver() const;

		inline hlim::Node_Signal* node() const { return m_node; }
		inline const std::shared_ptr<BitVectorSlice> &range() const { return m_range; }
		inline const Expansion &expansionPolicy() const { return m_expansionPolicy; }

		/// Needed to derive width from forward declared uint in SliceableBitVector template
		static size_t getUIntBitWidth(const UInt &uint);
	private:
		/// Signal node (potentially aliased) whose input represents this signal.
		hlim::NodePtr<hlim::Node_Signal> m_node;
		/// In case of a sliced bit vector, this range represents the offset and the width of the slice.
		std::shared_ptr<BitVectorSlice> m_range;
		BitWidth m_width;

		Expansion m_expansionPolicy = Expansion::none;

		std::vector<Bit>& aliasVec() const;
		Bit& aliasMsb() const;
		Bit& aliasLsb() const;

		mutable std::vector<Bit> m_bitAlias;
		mutable std::optional<Bit> m_lsbAlias;
		mutable std::optional<Bit> m_msbAlias;
		
		Bit &getDynamicBitAlias(const UInt &idx) const;
		mutable utils::UnstableMap<hlim::NodePort, Bit> m_dynamicBitAlias;

		mutable SignalReadPort m_readPort;
		mutable void* m_readPortDriver = nullptr;
	};

	inline BaseBitVector::iterator begin(BaseBitVector& bvec) { return bvec.begin(); }
	inline BaseBitVector::iterator end(BaseBitVector& bvec) { return bvec.end(); }
	inline BaseBitVector::const_iterator begin(const BaseBitVector& bvec) { return bvec.begin(); }
	inline BaseBitVector::const_iterator end(const BaseBitVector& bvec) { return bvec.end(); }

	template<class T>
	class SignalParts
	{
	public:
		class iterator
		{
		public:
			iterator(T& signal, size_t parts, size_t idx) : m_signal(signal), m_parts(parts), m_idx(idx) { }

			T& operator * () const { return m_signal.part(m_parts, m_idx); }
			T* operator -> () const { return &m_signal.part(m_parts, m_idx); }

			iterator& operator ++ () { ++m_idx; return *this; }
			iterator operator ++ (int) { iterator tmp(*this); ++m_idx; return tmp; }

			auto operator <=> (const iterator& rhs) const { return m_idx <=> rhs.m_idx; }
			bool operator == (const iterator& rhs) const { return m_idx == rhs.m_idx; }
			bool operator != (const iterator& rhs) const { return m_idx != rhs.m_idx; }

		private:
			T& m_signal;
			const size_t m_parts;
			size_t m_idx;
		};
	public:
		SignalParts(T& signal, size_t parts) : m_signal(signal), m_parts(parts) { }

		T& operator[](size_t idx) const			{ return at(idx); }
		T& operator[](const UInt& idx) const	{ return at(idx); }
		T& at(size_t idx) const					{ return m_signal.part(m_parts, idx); }
		T& at(const UInt& idx) const			{ return m_signal.part(m_parts, idx); }

		iterator begin() const { return iterator(m_signal, m_parts, 0); }
		iterator end() const { return iterator(m_signal, m_parts, m_parts); }
		size_t size() const { return m_parts; }

	private:
		T& m_signal;
		const size_t m_parts;
	};

	template<class T> typename SignalParts<T>::iterator begin(const SignalParts<T>& parts) { return parts.begin(); }
	template<class T> typename SignalParts<T>::iterator end(const SignalParts<T>& parts) { return parts.end(); }

	template<typename FinalType, typename DefaultValueType>
	class SliceableBitVector : public BaseBitVector
	{
	public:
		using OwnType = SliceableBitVector<FinalType, DefaultValueType>;

		using BaseBitVector::BaseBitVector;
		SliceableBitVector(OwnType&& rhs) : BaseBitVector(std::move(rhs)) { }
		SliceableBitVector(const OwnType &rhs) : BaseBitVector(rhs) { } // Necessary because otherwise deleted copy ctor takes precedence over templated ctor.

		FinalType& operator = (BitWidth width) { BaseBitVector::operator=(width); return (FinalType&)*this; }
		FinalType& operator = (const DefaultValueType &defaultValue) { BaseBitVector::operator=(defaultValue); return (FinalType&)*this; }

		void exportOverride(const FinalType& exportOverride) { BaseBitVector::exportOverride(exportOverride); }
		void simulationOverride(const FinalType& simulationOverride) { BaseBitVector::simulationOverride(simulationOverride); }

		FinalType& operator() (size_t offset, BitWidth size) { return (*this)(Selection::Slice(int(offset), int(size.value))); }
		const FinalType& operator() (size_t offset, BitWidth size) const { return (*this)(Selection::Slice(int(offset), int(size.value))); }
		FinalType& operator() (size_t offset, BitReduce reduction) { return (*this)(Selection::Slice(int(offset), int(BaseBitVector::size() - reduction.value))); }
		const FinalType& operator() (size_t offset, BitReduce reduction) const { return (*this)(Selection::Slice(int(offset), int(BaseBitVector::size() - reduction.value))); }

		FinalType& word(const UInt& index)				{ return aliasRange(BitVectorSlice(index, range())); }
		const FinalType& word(const UInt& index) const	{ return aliasRange(BitVectorSlice(index, range())); }
		FinalType& word(const size_t& index, BitWidth width) { return (*this)(index * width.bits(), width); }
		const FinalType& word(const size_t& index, BitWidth width) const { return (*this)(index * width.bits(), width); }

		FinalType& upper(BitWidth bits)					{ return (*this)((width() - bits).bits(), bits); }
		FinalType& upper(BitReduce bits)				{ return (*this)(bits.value, bits); }
		FinalType& lower(BitWidth bits)					{ return (*this)(0, bits); }
		FinalType& lower(BitReduce bits)				{ return (*this)(0, bits); }
		const FinalType& upper(BitWidth bits) const		{ return (*this)((width() - bits).bits(), bits); }
		const FinalType& upper(BitReduce bits) const	{ return (*this)(bits.value, bits);}
		const FinalType& lower(BitWidth bits) const		{ return (*this)(0, bits); }
		const FinalType& lower(BitReduce bits) const	{ return (*this)(0, bits); }

		SignalParts<FinalType> parts(size_t parts);
		SignalParts<const FinalType> parts(size_t parts) const;

		FinalType& part(size_t parts, const UInt& index);
		const FinalType& part(size_t parts, const UInt& index) const;
		FinalType& part(size_t parts, size_t index);
		const FinalType& part(size_t parts, size_t index) const;

		/// Slices a sub-vector out of the bit vector with a fixed width but a dynamic offset.
		template<std::same_as<UInt> To>
		FinalType& operator() (const To& offset, BitWidth size);
		/// Slices a sub-vector out of the bit vector with a fixed width but a dynamic offset.
		template<std::same_as<UInt> To>
		const FinalType& operator() (const To& offset, BitWidth size) const;

		FinalType& operator()(const Selection& selection);
		const FinalType& operator() (const Selection& selection) const;

		virtual void resetNode() override {
			BaseBitVector::resetNode();
			m_rangeAlias.clear();
		}
	protected:
		template<typename T>
		FinalType& aliasRange(std::shared_ptr<T> range) const {
			auto [it, exists] = m_rangeAlias.try_emplace(*range, node(), range, expansionPolicy(), m_initialScopeId);
			return it->second;
		}

		mutable std::map<std::variant<BitVectorSliceStatic, BitVectorSliceDynamic>, FinalType> m_rangeAlias;
	};

	template<BitVectorSignal T>
	T resizeTo(const T& vec, BitWidth width) { if(width > vec.width()) return ext(vec, width); else return vec(0, width); }

	template<typename FinalType, typename DefaultValueType>
	inline SignalParts<FinalType> SliceableBitVector<FinalType, DefaultValueType>::parts(size_t parts)
	{
		return SignalParts<FinalType>((FinalType&)*this, parts);
	}

	template<typename FinalType, typename DefaultValueType>
	inline SignalParts<const FinalType> SliceableBitVector<FinalType, DefaultValueType>::parts(size_t parts) const
	{
		return SignalParts<const FinalType>((const FinalType&)*this, parts);
	}

	template<typename FinalType, typename DefaultValueType>
	inline FinalType& SliceableBitVector<FinalType, DefaultValueType>::part(size_t parts, const UInt& index)
	{
		BitWidth partW = width() / parts;
		return aliasRange(
			std::make_shared<BitVectorSliceDynamic>(index, parts - 1, partW.bits(), partW, range())
		);
	}

	template<typename FinalType, typename DefaultValueType>
	inline const FinalType& SliceableBitVector<FinalType, DefaultValueType>::part(size_t parts, const UInt& index) const
	{
		BitWidth partW = width() / parts;
		return aliasRange(
			std::make_shared<BitVectorSliceDynamic>(index, parts - 1, partW.bits(), partW, range())
		);
	}

	template<typename FinalType, typename DefaultValueType>
	inline FinalType& SliceableBitVector<FinalType, DefaultValueType>::part(size_t parts, size_t index)
	{
		HCL_DESIGNCHECK(index < parts);
		return (*this)(index * (width() / parts).bits(), width() / parts);
	}

	template<typename FinalType, typename DefaultValueType>
	inline const FinalType& SliceableBitVector<FinalType, DefaultValueType>::part(size_t parts, size_t index) const
	{
		HCL_DESIGNCHECK(index < parts);
		return (*this)(index * (width() / parts).bits(), width() / parts);
	}

	template<typename FinalType, typename DefaultValueType>
	template<std::same_as<UInt> To>
	inline FinalType& SliceableBitVector<FinalType, DefaultValueType>::operator()(const To& offset, BitWidth size)
	{
		return aliasRange(
			std::make_shared<BitVectorSliceDynamic>(offset, ((BaseBitVector&)offset).width().last(), 1, size, range())
		);
	}

	template<typename FinalType, typename DefaultValueType>
	template<std::same_as<UInt> To>
	inline const FinalType& SliceableBitVector<FinalType, DefaultValueType>::operator()(const To& offset, BitWidth size) const
	{
		return aliasRange(
			std::make_shared<BitVectorSliceDynamic>(offset, ((BaseBitVector&)offset).width().last(), 1, size, range())
		);
	}

	template<typename FinalType, typename DefaultValueType>
	inline FinalType& SliceableBitVector<FinalType, DefaultValueType>::operator()(const Selection& selection)
	{
		return aliasRange(
			std::make_shared<BitVectorSliceStatic>(selection, width(), range())
		);
	}

	template<typename FinalType, typename DefaultValueType>
	inline const FinalType& SliceableBitVector<FinalType, DefaultValueType>::operator()(const Selection& selection) const
	{
		return aliasRange(
			std::make_shared<BitVectorSliceStatic>(selection, width(), range())
		);
	}
}
