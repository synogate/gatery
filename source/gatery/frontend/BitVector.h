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

#include <gatery/hlim/Attributes.h>

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
		size_t stride = 1;
		bool untilEndOfSource = false;

		static Selection All();
		static Selection From(int start);
		static Selection Range(int start, int end);
		static Selection RangeIncl(int start, int endIncl);
		static Selection StridedRange(int start, int end, size_t stride);

		static Selection Slice(size_t offset, size_t size);
		static Selection StridedSlice(int offset, int size, size_t stride);

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



    class BVecDefault {
        public:
            BVecDefault(const BVec& rhs);

			template <typename Int, typename = std::enable_if_t<std::is_integral_v<Int> & !std::is_same_v<Int, char> & !std::is_same_v<Int, bool>> >
			BVecDefault(Int vec) { assign(vec); }

            hlim::NodePort getNodePort() const { return m_nodePort; }
        protected:
			template <typename Int, typename = std::enable_if_t<std::is_integral_v<Int> & !std::is_same_v<Int, char> & !std::is_same_v<Int, bool>> >
			void assign(Int);
			void assign(std::string_view);

            hlim::RefCtdNodePort m_nodePort;
    };

	template <typename Int, typename>
	void BVecDefault::assign(Int value) {
		size_t width;
		//Expansion policy;

		if (value >= 0)
		{
			//policy = Expansion::zero;
			width = utils::Log2C(value + 1);
		}
		else
		{
			//policy = Expansion::sign;
			width = utils::Log2C(~value + 1) + 1;
		}

		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			parseBVec(uint64_t(value), width),
			hlim::ConnectionType::BITVEC
		);
		m_nodePort = {.node = constant, .port = 0ull };
	}


	class BVec : public ElementarySignal
	{
	public:
		struct Range {
			Range() = default;
			Range(const Range&) = default;
			Range(const Selection& s, const Range& r);

			auto operator <=> (const Range&) const = default;

			size_t bitOffset(size_t idx) const { HCL_ASSERT(idx < width); return offset + idx * stride; }

			size_t width = 0;
			size_t offset = 0;
			size_t stride = 1;
			bool subset = false;
		};

		using isBitVectorSignal = void;

		using iterator = std::vector<Bit>::iterator;
		using const_iterator = std::vector<Bit>::const_iterator;
		using reverse_iterator = std::vector<Bit>::reverse_iterator;
		using const_reverse_iterator = std::vector<Bit>::const_reverse_iterator;
		using value_type = Bit;

		BVec() = default;
		BVec(const BVec& rhs) { if(rhs.m_node) assign(rhs.getReadPort()); }
		BVec(BVec&& rhs);
		BVec(const BVecDefault &defaultValue);
		~BVec();

		BVec(const SignalReadPort& port) { assign(port); }
		BVec(hlim::Node_Signal* node, Range range, Expansion expansionPolicy, size_t initialScopeId); // alias
		BVec(BitWidth width, Expansion expansionPolicy = Expansion::none);

		template<unsigned S>
		BVec(const char(&rhs)[S]) { assign(std::string_view(rhs)); }

		template <typename Int, typename = std::enable_if_t<std::is_integral_v<Int> & !std::is_same_v<Int, char> & !std::is_same_v<Int, bool>> >
		BVec(Int vec) { assign(vec); }

		template <typename Int, typename = std::enable_if_t<std::is_integral_v<Int> & !std::is_same_v<Int, char> & !std::is_same_v<Int, bool>> >
		BVec& operator = (Int rhs) { assign(rhs); return *this; }
		template<unsigned S>
		BVec& operator = (const char (&rhs)[S]) { assign(std::string_view(rhs)); return *this; }
		BVec& operator = (const BVec& rhs) { if (rhs.m_node) assign(rhs.getReadPort()); return *this; }
		BVec& operator = (BVec&& rhs);
		BVec& operator = (BitWidth width);
		BVec& operator = (const BVecDefault &defaultValue);

		void setExportOverride(const BVec& exportOverride);
		void setAttrib(hlim::SignalAttributes attributes);

		template<typename Int1, typename Int2, typename = std::enable_if_t<std::is_integral_v<Int1> && std::is_integral_v<Int2>>>
		BVec& operator()(Int1 offset, Int2 size, size_t stride = 1) { return (*this)(Selection::StridedSlice(int(offset), int(size), stride)); }
		template<typename Int1, typename Int2, typename = std::enable_if_t<std::is_integral_v<Int1>&& std::is_integral_v<Int2>>>
		const BVec& operator() (Int1 offset, Int2 size, size_t stride = 1) const { return (*this)(Selection::StridedSlice(int(offset), int(size), stride)); }
		
		BVec& operator() (size_t offset, BitWidth size, size_t stride = 1);
		const BVec& operator() (size_t offset, BitWidth size, size_t stride = 1) const;

		BVec& operator()(const Selection& selection) { return aliasRange(Range(selection, m_range)); }
		const BVec& operator() (const Selection& selection) const { return aliasRange(Range(selection, m_range)); }


		virtual void resize(size_t width);

		Bit& lsb() { return aliasLsb(); }
		const Bit& lsb() const { return aliasLsb(); }
		Bit& msb() { return aliasMsb(); }
		const Bit& msb() const { return aliasMsb(); }

		Bit& operator[](size_t idx) { return aliasVec()[idx]; }
		const Bit& operator[](size_t idx) const { return aliasVec()[idx]; }

		Bit& at(size_t idx) { return aliasVec().at(idx); }
		const Bit& at(size_t idx) const { return aliasVec().at(idx); }

		bool empty() const { return size() == 0; }
		size_t size() const { return getWidth().value; }

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
		BitWidth getWidth() const final { return BitWidth{ m_range.width }; }
		hlim::ConnectionType getConnType() const final;
		SignalReadPort getReadPort() const final;
		SignalReadPort getOutPort() const final;
		std::string_view getName() const final;
		void setName(std::string name) override;
		void addToSignalGroup(hlim::SignalGroup *signalGroup);

	protected:
		template <typename Int, typename = std::enable_if_t<std::is_integral_v<Int> & !std::is_same_v<Int, char> & !std::is_same_v<Int, bool>> >
		void assign(Int);
		void assign(std::string_view);
		virtual void assign(SignalReadPort, bool ignoreConditions = false);

		void createNode(size_t width, Expansion policy);
		SignalReadPort getRawDriver() const;

	private:
		hlim::Node_Signal* m_node = nullptr;
		Range m_range;
		Expansion m_expansionPolicy = Expansion::none;

		std::vector<Bit>& aliasVec() const;
		Bit& aliasMsb() const;
		Bit& aliasLsb() const;
		BVec& aliasRange(const Range& range) const;

		mutable std::vector<Bit> m_bitAlias;
		mutable std::optional<Bit> m_lsbAlias;
		mutable std::optional<Bit> m_msbAlias;
		mutable std::map<Range, BVec> m_rangeAlias;

		mutable SignalReadPort m_readPort;
		mutable void* m_readPortDriver = nullptr;
	};

	inline BVec::iterator begin(BVec& bvec) { return bvec.begin(); }
	inline BVec::iterator end(BVec& bvec) { return bvec.end(); }
	inline BVec::const_iterator begin(const BVec& bvec) { return bvec.begin(); }
	inline BVec::const_iterator end(const BVec& bvec) { return bvec.end(); }

	BVec ext(const Bit& bvec, size_t increment);
	BVec ext(const Bit& bit, size_t increment, Expansion policy);
	inline BVec zext(const Bit& bit, size_t increment = 0) { return ext(bit, increment, Expansion::zero); }
	inline BVec oext(const Bit& bit, size_t increment = 0) { return ext(bit, increment, Expansion::one); }
	inline BVec sext(const Bit& bit, size_t increment = 0) { return ext(bit, increment, Expansion::sign); }

	BVec ext(const BVec& bvec, size_t increment);
	BVec ext(const BVec& bvec, size_t increment, Expansion policy);
	inline BVec zext(const BVec& bvec, size_t increment = 0) { return ext(bvec, increment, Expansion::zero); }
	inline BVec oext(const BVec& bvec, size_t increment = 0) { return ext(bvec, increment, Expansion::one); }
	inline BVec sext(const BVec& bvec, size_t increment = 0) { return ext(bvec, increment, Expansion::sign); }

	struct NormalizedWidthOperands
	{
		template<typename SigA, typename SigB>
		NormalizedWidthOperands(const SigA&, const SigB&);

		SignalReadPort lhs, rhs;
	};

	template<typename Int, typename>
	inline void BVec::assign(Int value)
	{
		size_t width;
		Expansion policy;

		if (value >= 0)
		{
			policy = Expansion::zero;
			width = utils::Log2C(value + 1);
		}
		else
		{
			policy = Expansion::sign;
			width = utils::Log2C(~value + 1) + 1;
		}

		auto* constant = DesignScope::createNode<hlim::Node_Constant>(
			parseBVec(uint64_t(value), width),
			hlim::ConnectionType::BITVEC
		);
		assign(SignalReadPort(constant, policy));
	}

	template<typename SigA, typename SigB>
	inline NormalizedWidthOperands::NormalizedWidthOperands(const SigA& l, const SigB& r)
	{
		lhs = l.getReadPort();
		rhs = r.getReadPort();

		const size_t maxWidth = std::max(width(lhs), width(rhs));

		hlim::ConnectionType::Interpretation type = hlim::ConnectionType::BITVEC;
		if (maxWidth == 1 &&
			(l.getConnType().interpretation != r.getConnType().interpretation ||
			 l.getConnType().interpretation == hlim::ConnectionType::BOOL))
			type = hlim::ConnectionType::BOOL;

		lhs = lhs.expand(maxWidth, type);
		rhs = rhs.expand(maxWidth, type);
	}

}
