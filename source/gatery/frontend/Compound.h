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
#include "BitVector.h"
#include "Reg.h"

#include <string_view>
#include <type_traits>

#include <boost/spirit/home/support/container.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/fwd/accessors.hpp>
#include <boost/hana/tuple.hpp>

namespace gtry
{
	class CompoundVisitor
	{
	public:
		virtual void enterPackStruct();
		virtual void enterPackContainer();
		virtual void leavePack();

		virtual void enter(std::string_view name);
		virtual void leave();

		virtual void operator () (const BVec& a, const BVec& b);
		virtual void operator () (BVec& a);
		virtual void operator () (BVec& a, const BVec& b);

		virtual void operator () (const Bit& a, const Bit& b);
		virtual void operator () (Bit& a);
		virtual void operator () (Bit& vec, const Bit& b);
	};

	class CompoundNameVisitor : public CompoundVisitor
	{
	public:
		virtual void enter(std::string_view name) override;
		virtual void leave() override;

		std::string makeName() const;
	protected:
		std::vector<std::string_view> m_names;
	};

	template<typename T, typename En = void>
	struct VisitCompound
	{
		// simply assign "meta data" (members that are not and do not contain signals)
		void operator () (T& a, const T& b, CompoundVisitor&, size_t flags) { a = b; }
		void operator () (T& a, CompoundVisitor&) {}
		void operator () (const T&, const T&, CompoundVisitor&) {}
	};

	template<>
	struct VisitCompound<BVec>
	{
		void operator () (BVec& a, const BVec& b, CompoundVisitor& v, size_t flags) { v(a, b); }
		void operator () (BVec& a, CompoundVisitor& v) { v(a); }
		void operator () (const BVec& a, const BVec& b, CompoundVisitor& v) { v(a, b); }
	};

	template<>
	struct VisitCompound<Bit>
	{
		void operator () (Bit& a, const Bit& b, CompoundVisitor& v, size_t flags) { v(a, b); }
		void operator () (Bit& a, CompoundVisitor& v) { v(a); }
		void operator () (const Bit& a, const Bit& b, CompoundVisitor& v) { v(a, b); }
	};

	namespace internal
	{
		template <typename T, typename = int>
		struct resizable : std::false_type {};

		template <typename T>
		struct resizable <T, decltype((void)std::declval<T>().resize(1), 0)> : std::true_type {};


		template<typename T, typename = int>
		struct is_signal : std::false_type {
			using sig_type = T;
		};

		template<typename T>
		struct is_signal<T, decltype((void)BVec{ std::declval<T>() }, 0)> : std::true_type {
			using sig_type = BVec;
		};

		template<typename T>
		struct is_signal<T, decltype((void)Bit{ std::declval<T>() }, 0)> : std::true_type {
			using sig_type = Bit;
		};

		template<typename T, typename = std::enable_if_t<!is_signal<T>::value>>
		const T& signalOTron(const T& ret) { return ret; }
		inline const BVec& signalOTron(const BVec& vec) { return vec; }
		inline const Bit& signalOTron(const Bit& bit) { return bit; }
	}

	template<typename T>
	void visitForcedSignalCompound(const T& sig, CompoundVisitor& v)
	{
		VisitCompound<typename internal::is_signal<T>::sig_type>{}(
			internal::signalOTron(sig),
			internal::signalOTron(sig),
			v
		);
	}

	template<typename... T>
	struct VisitCompound<std::tuple<T...>>
	{
		template<typename TComp>
		static constexpr std::string_view usableName()
		{
			std::string_view name = typeid(TComp).name();

			auto pos_s = name.find("::", 0);
			if (pos_s == std::string_view::npos)
				pos_s = 0;
			else
				pos_s += 2;

			auto pos_e = pos_s;
			do
			{
				if (name[pos_e] >= 'a' && name[pos_e] <= 'z')
					continue;
				if (name[pos_e] >= 'A' && name[pos_e] <= 'Z')
					continue;
				if (pos_e == pos_s)
					break;
				if (name[pos_e] >= '0' && name[pos_e] <= '9')
					continue;
				if (name[pos_e] == '_')
					continue;
				if (name[pos_e] == '-')
					continue;
				break;
			} while (++pos_e < name.length());

			return name.substr(pos_s, pos_e - pos_s);
		}

		void operator () (std::tuple<T...>& a, const std::tuple<T...>& b, CompoundVisitor& v, size_t flags)
		{
			v.enterPackStruct();
			auto zipped = boost::hana::zip_with([](auto&&... args) { return std::tie(args...); }, a, b);
			boost::hana::for_each(zipped, [&](auto& elem) {
				v.enter(usableName<decltype(std::get<0>(elem))>());
				VisitCompound<std::remove_cvref_t<decltype(std::get<0>(elem))>>{}(
					std::get<0>(elem), std::get<1>(elem), v, flags
				);
				v.leave();
			});
			v.leavePack();
		}

		void operator () (std::tuple<T...>& a, CompoundVisitor& v)
		{
			v.enterPackStruct();
			boost::hana::for_each(a, [&](auto& elem) {
				v.enter(usableName<decltype(elem)>());
				VisitCompound<std::remove_cvref_t<decltype(elem)>>{}(elem, v);
				v.leave();
			});
			v.leavePack();
		}

		void operator () (const std::tuple<T...>& a, const std::tuple<T...>& b, CompoundVisitor& v)
		{
			v.enterPackStruct();
			auto zipped = boost::hana::zip_with([](auto&&... args) { return std::tie(args...); }, a, b);
			boost::hana::for_each(zipped, [&](auto& elem) {
				v.enter(usableName<decltype(std::get<0>(elem))>());
				VisitCompound<std::remove_cvref_t<decltype(std::get<0>(elem))>>{}(
					std::get<0>(elem), std::get<1>(elem), v
				);
				v.leave();
			});
			v.leavePack();
		}
	};

	template<typename T>
	struct VisitCompound<T, std::enable_if_t<boost::spirit::traits::is_container<T>::value>>
	{
		void operator () (T& a, const T& b, CompoundVisitor& v, size_t flags)
		{
			if constexpr (internal::resizable<T>::value)
				if (a.size() != b.size())
					a.resize(b.size());

			auto it_a = begin(a);
			auto it_b = begin(b);

			std::string idx_string;
			size_t idx = 0;

			v.enterPackContainer();
			for (; it_a != end(a) && it_b != end(b); it_a++, it_b++)
			{
				v.enter(idx_string = std::to_string(idx++));
				VisitCompound<std::remove_cvref_t<decltype(*it_a)>>{}(*it_a, *it_b, v, flags);
				v.leave();
			}
			v.leavePack();

			if (it_a != end(a) || it_b != end(b))
				throw std::runtime_error("visit compound container of unequal size");
		}

		void operator () (T& a, CompoundVisitor& v)
		{
			std::string idx_string;
			size_t idx = 0;

			v.enterPackContainer();
			for (auto& it : a)
			{
				v.enter(idx_string = std::to_string(idx++));
				VisitCompound<std::remove_cvref_t<decltype(it)>>{}(it, v);
				v.leave();
			}
			v.leavePack();
		}

		void operator () (const T& a, const T& b, CompoundVisitor& v) 
		{
			auto it_a = begin(a);
			auto it_b = begin(b);

			std::string idx_string;
			size_t idx = 0;

			v.enterPackContainer();
			for (; it_a != end(a) && it_b != end(b); it_a++, it_b++)
			{
				v.enter(idx_string = std::to_string(idx++));
				VisitCompound<std::remove_cvref_t<decltype(*it_a)>>{}(*it_a, *it_b, v);
				v.leave();
			}
			v.leavePack();

			if (it_a != end(a) || it_b != end(b))
				throw std::runtime_error("visit compound container of unequal size");
		}
	};

	template<typename T>
	struct VisitCompound<T, std::enable_if_t<boost::hana::Struct<T>::value>>
	{
		void operator () (T& a, const T& b, CompoundVisitor& v, size_t flags)
		{
			v.enterPackStruct();
			boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto member) {
				auto& suba = boost::hana::second(member)(a);
				auto& subb = boost::hana::second(member)(b);

				v.enter(boost::hana::first(member).c_str());
				VisitCompound<std::remove_cvref_t<decltype(suba)>>{}(suba, subb, v, flags);
				v.leave();
				});
			v.leavePack();
		}

		void operator () (T& a, CompoundVisitor& v)
		{
			v.enterPackStruct();
			boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto member) {
				auto& suba = boost::hana::second(member)(a);

				v.enter(boost::hana::first(member).c_str());
				VisitCompound<std::remove_cvref_t<decltype(suba)>>{}(suba, v);
				v.leave();
				});
			v.leavePack();
		}

		void operator () (const T& a, const T& b, CompoundVisitor& v)
		{
			v.enterPackStruct();
			boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto member) {
				auto& suba = boost::hana::second(member)(a);
				auto& subb = boost::hana::second(member)(b);

				v.enter(boost::hana::first(member).c_str());
				VisitCompound<std::remove_cvref_t<decltype(suba)>>{}(suba, subb, v);
				v.leave();
				});
			v.leavePack();
		}
	};

	template<typename... Comp>
	BitWidth width(const Comp& ... compound)
	{
		struct WidthVisitor : CompoundVisitor
		{
			void operator () (const BVec& vec, const BVec&) final {
				m_totalWidth += vec.size();
			}

			void operator () (const Bit&, const Bit&) final {
				m_totalWidth++;
			}

			size_t m_totalWidth = 0;
		};

		WidthVisitor v;
		(VisitCompound<Comp>{}(compound, compound, v), ...);
		return BitWidth{ v.m_totalWidth };
	}

	template<typename Comp>
	void setName(Comp& compound, std::string_view prefix)
	{
		struct NameVisitor : CompoundNameVisitor
		{
			void operator () (BVec& vec) override { vec.setName(makeName()); }
			void operator () (Bit& vec) override { vec.setName(makeName()); }
		};

		NameVisitor v;
		v.enter(prefix);
		VisitCompound<Comp>{}(compound, v);
		v.leave();
	}

	void setName(const Bit&, std::string_view) = delete;
	void setName(const BVec&, std::string_view) = delete;

	template<typename T>
	struct Reg<T, std::enable_if_t<boost::spirit::traits::is_container<T>::value>>
	{
		T operator () (const T& signal)
		{
			T ret = signal;
			for (auto& item : ret)
				item = reg(item);
			return ret;
		}

		T operator () (const T& signal, const T& reset)
		{
			T ret = signal;

			auto itS = begin(ret);
			auto itR = begin(reset);
			for (; itS != end(ret) && itR != end(reset); ++itR, ++itS)
				*itS = reg(*itS, *itR);
			for (; itS != end(ret); ++itS)
				*itS = reg(*itS);
			return ret;
		}
	};

	template<typename T>
	struct Reg<T, std::enable_if_t<boost::hana::Struct<T>::value>>
	{
		T operator () (const T& signal)
		{
			T ret = signal;
			boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto&& member) {
				auto& subSig = boost::hana::second(member)(ret);
				subSig = reg(subSig);
			});
			return ret;
		}

		T operator () (const T& signal, const T& reset)
		{
			T ret = signal;
			boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto member) {
				auto& subSig = boost::hana::second(member)(ret);
				const auto& subResetSig = boost::hana::second(member)(reset);
				subSig = reg(subSig, subResetSig);
				});
			return ret;
		}
	};

}
