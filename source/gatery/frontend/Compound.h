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
#include "BitVector.h"
#include "../utils/Traits.h"

#include "ClangTupleWorkaround.h"

#include <string_view>
#include <type_traits>

#include <boost/spirit/home/support/container.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/fwd/accessors.hpp>
#include <boost/hana/tuple.hpp>

#include <boost/hana/ext/std/array.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/ext/std/pair.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/zip.hpp>
#include <boost/pfr.hpp>

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

		virtual void reverse();

		virtual void operator () (const ElementarySignal& a, const ElementarySignal& b) { }
		virtual void operator () (ElementarySignal& a) { }
		virtual void operator () (ElementarySignal& a, const ElementarySignal& b) { }
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



	namespace internal
	{
		// Forward all meta data
		template<typename T> requires (!BaseSignalValue<T>)
		const T& signalOTron(const T& ret) { return ret; }

		// Forward all signals without copy or conversion
		template<BaseSignal T>
		inline const T& signalOTron(const T& vec) { return vec; }

		// Convert everything that can be converted to a signal
		template<BaseSignalValue T> requires (!BaseSignal<T>)
		auto signalOTron(const T& ret) { return ValueToBaseSignal<T>{ret}; }
	}

	template<typename T>
	void visitForcedSignalCompound(const T& sig, CompoundVisitor& v)
	{
		VisitCompound<ValueToBaseSignal<T>>{}(
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
			if constexpr (resizable<T>::value)
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

	template<typename T>
	struct VisitCompound<std::optional<T>>
	{
		void operator () (std::optional<T>& a, const std::optional<T>& b, CompoundVisitor& v, size_t flags)
		{
			if (b && !a)
				a.emplace(constructFrom(b));

			if (b)
				VisitCompound<std::remove_cvref_t<T>>{}(*a, *b, v, flags);
		}

		void operator () (std::optional<T>& a, CompoundVisitor& v)
		{
			if (a)
				VisitCompound<std::remove_cvref_t<T>>{}(*a, v);
		}

		void operator () (const std::optional<T>& a, const std::optional<T>& b, CompoundVisitor& v)
		{
			if(a && b)
				VisitCompound<std::remove_cvref_t<T>>{}(*a, *b, v);
		}
	};

	template<typename Comp>
	void setName(Comp& compound, std::string_view prefix)
	{
		struct NameVisitor : CompoundNameVisitor
		{
			void operator () (ElementarySignal& vec) override { vec.setName(makeName()); }
		};

		NameVisitor v;
		v.enter(prefix);
		VisitCompound<Comp>{}(compound, v);
		v.leave();
	}

	void setName(const Bit&, std::string_view) = delete;
	void setName(const UInt&, std::string_view) = delete;

	namespace internal
	{

		template<BaseSignal T, typename TFunc>
		T transformSignal(const T& val, TFunc&& func);

		template<BaseSignal T, std::convertible_to<T> Tr, typename TFunc>
		T transformSignal(const T& val, const Tr& resetVal, TFunc&& func);

		template<CompoundSignal T, typename TFunc>
		T transformSignal(const T& val, TFunc&& func);

		template<CompoundSignal T, std::convertible_to<T> Tr, typename TFunc>
		T transformSignal(const T& val, const Tr& resetVal, TFunc&& func);

		template<ContainerSignal T, typename TFunc>
		T transformSignal(const T& val, TFunc&& func);

		template<ContainerSignal T, ContainerSignal Tr, typename TFunc>
		T transformSignal(const T& val, const Tr& resetVal, TFunc&& func);

		template<TupleSignal T, typename TFunc>
		T transformSignal(const T& val, TFunc&& func);

		template<TupleSignal T, TupleSignal Tr, typename TFunc>
		T transformSignal(const T& val, const Tr& resetVal, TFunc&& func);

		template<typename T, typename TFunc>
		T transformIfSignal(const T& signal, TFunc&& func)
		{
			if constexpr(Signal<T>)
				return transformSignal(signal, func);
			else
				return signal;
		}

		template<typename T, typename Tr, typename TFunc>
		T transformIfSignal(const T& signal, Tr&& secondSignal, TFunc&& func)
		{
			if constexpr(Signal<T>)
				return transformSignal(signal, secondSignal, func);
			else
				return T{ secondSignal };
		}

		template<BaseSignal T, typename TFunc>
		T transformSignal(const T& val, TFunc&& func)
		{
			return func(val);
		}

		template<BaseSignal T, std::convertible_to<T> Tr, typename TFunc>
		T transformSignal(const T& val, const Tr& resetVal, TFunc&& func)
		{
			return func(val, resetVal);
		}

		template<CompoundSignal T, typename TFunc>
		T transformSignal(const T& val, TFunc&& func)
		{
			return gtry::make_from_tuple<T>(
				boost::hana::transform(boost::pfr::structure_tie(val), [&](auto&& member) {
					if constexpr(Signal<decltype(member)>)
						return transformSignal(member, func);
					else
						return member;
				})
			);
		}

		template<CompoundSignal T, std::convertible_to<T> Tr, typename TFunc>
		T transformSignal(const T& val, const Tr& resetVal, TFunc&& func)
		{
			const T& resetValT = resetVal;
			return gtry::make_from_tuple<T>(
				boost::hana::transform(
				boost::hana::zip_with([](auto&& a, auto&& b) { return std::tie(a, b); },
					boost::pfr::structure_tie(val),
					boost::pfr::structure_tie(resetValT)
				), 
				[&](auto member) {
					if constexpr(Signal<decltype(std::get<0>(member))>)
						return transformSignal(std::get<0>(member), std::get<1>(member), func);
					else
						return std::get<1>(member); // use reset value for metadata

				})
			);
		}

		template<ContainerSignal T, typename TFunc>
		T transformSignal(const T& val, TFunc&& func)
		{
			T ret;

			if constexpr(requires { ret.reserve(val.size()); })
				ret.reserve(val.size());

			for(auto&& it : val)
				ret.insert(ret.end(), transformSignal(it, func));

			return ret;
		}

		template<ContainerSignal T, ContainerSignal Tr, typename TFunc>
		T transformSignal(const T& val, const Tr& resetVal, TFunc&& func)
		{
			HCL_DESIGNCHECK(val.size() == resetVal.size());

			T ret;

			if constexpr(requires { ret.reserve(val.size()); })
				ret.reserve(val.size());

			auto it_reset = resetVal.begin();
			for(auto&& it : val)
				ret.insert(ret.end(), transformSignal(it, *it_reset++, func));

			return ret;
		}

		template<TupleSignal T, typename TFunc>
		T transformSignal(const T& val, TFunc&& func)
		{
			using namespace internal;

			auto in2 = boost::hana::unpack(val, [](auto&&... args) {
				return std::tie(args...);
			});

			return boost::hana::unpack(in2, [&](auto&&... args) {
				return T{ transformIfSignal(args, func)... };
			});
		}

		template<TupleSignal T, TupleSignal Tr, typename TFunc>
		T transformSignal(const T& val, const Tr& resetVal, TFunc&& func)
		{
			static_assert(std::tuple_size<T>::value == std::tuple_size<Tr>::value, "Mismatch between signal and reset value tuple size");

			using namespace internal;

			// Turn tuples into tuples of lvalue references
			auto in2 = boost::hana::unpack(val, [](auto&&... args) {
				return std::tie(args...);
			});
			auto in2r = boost::hana::unpack(resetVal, [](auto&&... args) {
				return std::tie(args...);
			});

			static_assert(std::tuple_size<decltype(in2)>::value == std::tuple_size<decltype(in2r)>::value, "Internal gatery error, something got lost uppon transforming signals");

			// Zip in2 and in2r into:
			// tuple{
			//		tuple{ val[0], resetVal[0] },
			//		tuple{ val[1], resetVal[1] },
			//		tuple{ val[2], resetVal[2] },
			//  ....
			// }
			// all of which l value references.

			auto in3 = boost::hana::zip_with([](auto&& a, auto&& b) {
				static_assert(std::is_constructible_v<decltype(a), decltype(b)>, "reset type is not convertable to signal type");
				return std::tie(a, b);
			}, in2, in2r);

			static_assert(std::tuple_size<decltype(in2)>::value == std::tuple_size<decltype(in3)>::value, "Internal gatery error, something got lost uppon transforming signals");


			static_assert(std::is_same_v<decltype(boost::hana::at_c<0>(in3)), decltype(std::get<0>(in3))>);

			// Essentially return Tuple{ transform(in3[0][0], in3[0][1]), transform(in3[1][0], in3[1][1]), ...}
			return boost::hana::unpack(in3, [&](auto&&... args) {
				return T{ transformIfSignal(std::get<0>(args), std::get<1>(args), func)...};
			});
		}
	}


	namespace internal
	{
		void width(const BaseSignal auto& signal, BitWidth& sum);
		void width(const ContainerSignal auto& signal, BitWidth& sum);
		void width(const CompoundSignal auto& signal, BitWidth& sum);
		void width(const TupleSignal auto& signal, BitWidth& sum);

		void width(const BaseSignal auto& signal, BitWidth& sum)
		{
			sum += signal.width();
		}

		void width(const ContainerSignal auto& signal, BitWidth& sum)
		{
			for(auto& it : signal)
				width(it, sum);
		}

		void width(const CompoundSignal auto& signal, BitWidth& sum)
		{
			width(boost::pfr::structure_tie(signal), sum);
		}

		void width(const TupleSignal auto& signal, BitWidth& sum)
		{
			boost::hana::for_each(signal, [&](const auto& member) {
				if constexpr(Signal<decltype(member)>)
					width(member, sum);
			});
		}
	}

	BitWidth width(const Signal auto& ...args)
	{
		BitWidth sum;
		(internal::width(args, sum), ...);
		return sum;
	}
}


#include "Bit.h"
#include "UInt.h"
#include "SInt.h"
#include "BVec.h"
#include "Enum.h"

namespace gtry
{

	template<>
	struct VisitCompound<BVec>
	{
		void operator () (BVec& a, const BVec& b, CompoundVisitor& v, size_t flags) { v(a, b); }
		void operator () (BVec& a, CompoundVisitor& v) { v(a); }
		void operator () (const BVec& a, const BVec& b, CompoundVisitor& v) { v(a, b); }
	};

	template<>
	struct VisitCompound<UInt>
	{
		void operator () (UInt& a, const UInt& b, CompoundVisitor& v, size_t flags) { v(a, b); }
		void operator () (UInt& a, CompoundVisitor& v) { v(a); }
		void operator () (const UInt& a, const UInt& b, CompoundVisitor& v) { v(a, b); }
	};

	template<>
	struct VisitCompound<SInt>
	{
		void operator () (SInt& a, const SInt& b, CompoundVisitor& v, size_t flags) { v(a, b); }
		void operator () (SInt& a, CompoundVisitor& v) { v(a); }
		void operator () (const SInt& a, const SInt& b, CompoundVisitor& v) { v(a, b); }
	};

	template<>
	struct VisitCompound<Bit>
	{
		void operator () (Bit& a, const Bit& b, CompoundVisitor& v, size_t flags) { v(a, b); }
		void operator () (Bit& a, CompoundVisitor& v) { v(a); }
		void operator () (const Bit& a, const Bit& b, CompoundVisitor& v) { v(a, b); }
	};

	template<EnumType T>
	struct VisitCompound<Enum<T>>
	{
		void operator () (Enum<T>& a, const Enum<T>& b, CompoundVisitor& v, size_t flags) { v(a, b); }
		void operator () (Enum<T>& a, CompoundVisitor& v) { v(a); }
		void operator () (const Enum<T>& a, const Enum<T>& b, CompoundVisitor& v) { v(a, b); }
	};

}
