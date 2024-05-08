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
#include <span>
#include <concepts>

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
#include <gatery/compat/boost_pfr.h>

namespace gtry
{
	template<typename T>
	concept IsHanaAdapted = requires() {
		{boost::hana::accessors_impl<T>::apply() };
	};


	#if 0
	// Provide a specialization for structure_tie that is faster than boost pfr:
	template<typename T> requires(IsHanaAdapted<T>)
	constexpr auto structure_tie(T &t) {
		return boost::hana::unpack(boost::hana::accessors<T>(), [&](auto... Member) {
					return std::tie(boost::hana::second(Member)(t)...);
				});	
	}
	template<typename T> requires(IsHanaAdapted<T>)
	constexpr auto structure_tie(const T &t) {
		return boost::hana::unpack(boost::hana::accessors<T>(), [&](auto... Member) {
					return std::tie(boost::hana::second(Member)(t)...);
				});	
	}

	// Fallback:
	template<typename T> requires(!IsHanaAdapted<T>)
	constexpr auto structure_tie(T &t) { return boost::pfr::structure_tie(t); }
	template<typename T> requires(!IsHanaAdapted<T>)
	constexpr auto structure_tie(const T &t) { return boost::pfr::structure_tie(t); }
	#else
	template<typename T>
	constexpr auto structure_tie(T &t) { return boost::pfr::structure_tie(t); }
	template<typename T>
	constexpr auto structure_tie(const T &t) { return boost::pfr::structure_tie(t); }
	#endif

	struct CompoundMemberAnnotation {
		std::string_view shortDesc;
	};

	struct CompoundAnnotation {
		std::string_view shortDesc;
		std::string_view longDesc;
		std::vector<CompoundMemberAnnotation> memberDesc;
	};

	template<typename T>
	struct CompoundAnnotator {
	};

	template<typename T>
	concept IsAnnotated = requires(T) { {gtry::CompoundAnnotator<T>::annotation} -> std::convertible_to<const CompoundAnnotation&>; };

	template<typename T>
	consteval CompoundAnnotation* getAnnotation() { return nullptr; }

	template<typename T> requires (IsAnnotated<T>)
	consteval CompoundAnnotation* getAnnotation() { return &gtry::CompoundAnnotator<T>::annotation; }


	/**
	 * @brief Concept for a visitor for performing operations on all members of (potential) compounds recursively.
	 * @details The visitor must be able to deal with all terminal members (int, BVec, ...).
	 * Handling of non-terminal members (structs, containers, tuples) is handled by VisitCompound.
	 * That is, the actual recursion is implemented in VisitCompound so that the Visitors can focus on the 
	 * member operations.
	 */
	// made ElementarySignal a reference because using an abstract class as a parameter didn't work
	template<typename T>
	concept CompoundVisitor = requires(T v, std::string_view name, const int i, ElementarySignal& a, std::array<int, 5> container, std::tuple<int, int> myStruct) {
		{ v.enterPackStruct(myStruct) } -> std::convertible_to<bool>;
		{ v.enterPackContainer(container) } -> std::convertible_to<bool>;
		v.leavePack();
		v.enter(i, name);
		v.enter(a, name);
		v.leave();
		v.reverse();
	};
	
	template<typename T>
	concept CompoundUnaryVisitor = CompoundVisitor<T> && requires(T v, ElementarySignal& a, int i) {
		v(i);
		v(a);
	};

	template<typename T>
	concept CompoundBinaryVisitor = CompoundVisitor<T> && requires(T v, const ElementarySignal& a, const int i) {
		v(i, i);
		v(a, a);
	};

	template<typename T>
	concept CompoundAssignmentVisitor = CompoundVisitor<T> && requires(T v, const ElementarySignal& a, ElementarySignal& b, int i, const int j) {
		v(i, j);
		v(b, a);
	};

	/**
	 * @brief Helper implementation of the CompoundVisitor concept that ignores all callbacks.
	 * @details This helps with the implementation of dedicated visitors by allowing through inheritance to only implement
	 * the needed callbacks.
	 */
	class DefaultCompoundVisitor
	{
	public:
		/**
		 * @brief Entering a member variable with a given name.
		 * @details Calls to enter and leave can frame calls to operator(...) or the ycan frame hierarchy jumps through enterPackXXX(...) and leavePack(...)
		 * and are used to signal member names.
		 * @param t The member variable.
		 * @param name The name of the member variable.
		 */
		template<typename T>
		void enter(const T &t, std::string_view name) { enterName(name); }
		/// Counterpart to enter()
		void leave() { }

		/**
		 * @brief Entering a struct or class and returns with indications on how to proceed with member enumeration.
		 * @details The return value indicates whether the automatic exploration of members can reccurse into the struct
		 * or whether the struct as a whole is to be passed to the visitor's operator(...).
		 * @param t The struct about to be entered
		 * @returns True if the reccursion is to enter the struct and false, if the struct is to be passed to operator(...) as a whole.
		 */
		template<typename T>
		bool enterPackStruct(const T &t) { return true; }

		/**
		 * @brief Entering a container and returns with indications on how to proceed with element enumeration.
		 * @details The return value indicates whether the automatic exploration of element can reccurse into the container
		 * or whether the container as a whole is to be passed to the visitor's operator(...).
		 * @param t The container about to be entered
		 * @returns True if the reccursion is to enter the container and false, if the container is to be passed to operator(...) as a whole.
		 */
		template<typename T>
		bool enterPackContainer(const T &t) { return true; }

		/// Counterpart to enterPackStruct and enterPackContainer
		void leavePack() { }

		/// Called when recursing through a Reverse<> signal (and also on the way back out).
		void reverse() { }

		template<typename T>
		void operator () (const T& a, const T& b) { 
			if constexpr (std::derived_from<T, ElementarySignal>)
				elementaryOnly((const ElementarySignal&)a, (const ElementarySignal&)b);
		}

		template<typename T>
		void operator () (T& a) {
			if constexpr (std::derived_from<T, ElementarySignal>)
				elementaryOnly((ElementarySignal&)a);
		}

		template<typename T>
		void operator () (T& a, const T& b) {
			if constexpr (std::derived_from<T, ElementarySignal>)
				elementaryOnly((ElementarySignal&)a, (const ElementarySignal&)a);
			else
				// simply assign "meta data" (members that are not and do not contain signals)
				a = b;
		}

		/// Simplified, non-template version of enter(...) that only reports the name.
		virtual void enterName(std::string_view name) { }
		/// Simplified, non-template version of the operator(...) that is called if the member variable is an ElementarySignal
		virtual void elementaryOnly(const ElementarySignal& a, const ElementarySignal& b) { }
		/// Simplified, non-template version of the operator(...) that is called if the member variable is an ElementarySignal
		virtual void elementaryOnly(ElementarySignal& a) { }
		/// Simplified, non-template version of the operator(...) that is called if the member variable is an ElementarySignal
		virtual void elementaryOnly(ElementarySignal& a, const ElementarySignal& b) { }
	};

	class CompoundNameVisitor : public DefaultCompoundVisitor
	{
	public:
		virtual void enterName(std::string_view name) override;
		void leave();

		std::string makeName() const;
	protected:
		std::vector<std::string_view> m_names;
	};

	/**
	 * @brief Handles the recursive iteration over the members of (potential) compounds.
	 * @details The actual operations on the terminal members (int, BVec, ...) are performed
	 * by a Visitor which no longer needs to handle the recursive iteration, but still needs to
	 * be able to handle all types of members (except structs, tuples, and containers).
	 */
	template<typename T, typename En = void>
	struct VisitCompound
	{
		template<CompoundAssignmentVisitor Visitor>
		void operator () (T& a, const T& b, Visitor& v) { v(a, b); }

		template<CompoundUnaryVisitor Visitor>
		void operator () (T& a, Visitor& v) { v(a); }

		template<CompoundBinaryVisitor Visitor>
		void operator () (const T& a, const T& b, Visitor& v) { v(a, b); }
	};

	/// Reccursively iterate over all (pairs of) members of a compound and invoke the visitor's callbacks on the way for hierarchy transitions and terminal members (i.e. int, BVec, ...).
	template<typename T, CompoundAssignmentVisitor Visitor>
	void reccurseCompoundMembers(T& a, const T& b, Visitor& v, std::string_view prefix) {
		v.enter(a, prefix);
		VisitCompound<T>{}(a, b, v);
		v.leave();
	}

	/// Reccursively iterate over all members of a compound and invoke the visitor's callbacks on the way for hierarchy transitions and terminal members (i.e. int, BVec, ...).
	template<typename T, CompoundUnaryVisitor Visitor>
	void reccurseCompoundMembers(T& a, Visitor& v, std::string_view prefix) {
		v.enter(a, prefix);
		VisitCompound<T>{}(a, v);
		v.leave();
	}

	/// Reccursively iterate over all (pairs of) members of a compound and invoke the visitor's callbacks on the way for hierarchy transitions and terminal members (i.e. int, BVec, ...).
	template<typename T, CompoundBinaryVisitor Visitor>
	void reccurseCompoundMembers(const T& a, const T& b, Visitor& v, std::string_view prefix) {
		v.enter(a, prefix);
		VisitCompound<T>{}(a, b, v);
		v.leave();
	}


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

/*
	template<typename T>
	void visitForcedSignalCompound(const T& sig, CompoundVisitor auto& v)
	{
		VisitCompound<ValueToBaseSignal<T>>{}(
			internal::signalOTron(sig),
			internal::signalOTron(sig),
			v
		);
	}
*/

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

		template<CompoundAssignmentVisitor Visitor>
		void operator () (std::tuple<T...>& a, const std::tuple<T...>& b, Visitor& v)
		{
			if (v.enterPackStruct(a)) {
				auto zipped = boost::hana::zip_with([](auto&&... args) { return std::tie(args...); }, a, b);
				boost::hana::for_each(zipped, [&](auto& elem) {
					v.enter(std::get<0>(elem), usableName<decltype(std::get<0>(elem))>());
					VisitCompound<std::remove_cvref_t<decltype(std::get<0>(elem))>>{}(
						std::get<0>(elem), std::get<1>(elem), v
					);
					v.leave();
				});
			} else
				v(a, b);
			v.leavePack();
		}

		template<CompoundUnaryVisitor Visitor>
		void operator () (std::tuple<T...>& a, Visitor& v)
		{
			if (v.enterPackStruct(a))
				boost::hana::for_each(a, [&](auto& elem) {
					v.enter(elem, usableName<decltype(elem)>());
					VisitCompound<std::remove_cvref_t<decltype(elem)>>{}(elem, v);
					v.leave();
				});
			else
				v(a);
			v.leavePack();
		}

		template<CompoundBinaryVisitor Visitor>
		void operator () (const std::tuple<T...>& a, const std::tuple<T...>& b, Visitor& v)
		{
			if (v.enterPackStruct(a)) {
				auto zipped = boost::hana::zip_with([](auto&&... args) { return std::tie(args...); }, a, b);
				boost::hana::for_each(zipped, [&](auto& elem) {
					v.enter(std::get<0>(elem), usableName<decltype(std::get<0>(elem))>());
					VisitCompound<std::remove_cvref_t<decltype(std::get<0>(elem))>>{}(
						std::get<0>(elem), std::get<1>(elem), v
					);
					v.leave();
				});
			} else
				v(a);
			v.leavePack();
		}
	};

	template<typename T>
	struct VisitCompound<T, std::enable_if_t<boost::spirit::traits::is_container<T>::value>>
	{
		template<CompoundAssignmentVisitor Visitor>
		void operator () (T& a, const T& b, Visitor& v)
		{
			if constexpr (resizable<T>::value)
				if (a.size() != b.size())
					a.resize(b.size());

			auto it_a = begin(a);
			auto it_b = begin(b);

			std::string idx_string;
			size_t idx = 0;

			if (v.enterPackContainer(a)) {
				for (; it_a != end(a) && it_b != end(b); it_a++, it_b++)
				{
					v.enter(*it_a, idx_string = std::to_string(idx++));
					VisitCompound<std::remove_cvref_t<decltype(*it_a)>>{}(*it_a, *it_b, v);
					v.leave();
				}
			} else
				v(a, b);
			v.leavePack();

			if (it_a != end(a) || it_b != end(b))
				throw std::runtime_error("visit compound container of unequal size");
		}

		template<CompoundUnaryVisitor Visitor>
		void operator () (T& a, Visitor& v)
		{
			std::string idx_string;
			size_t idx = 0;

			if (v.enterPackContainer(a))
				for (auto& element : a)
				{
					v.enter(element, idx_string = std::to_string(idx++));
					VisitCompound<std::remove_cvref_t<decltype(element)>>{}(element, v);
					v.leave();
				}
			else
				v(a);
			v.leavePack();
		}

		template<CompoundBinaryVisitor Visitor>
		void operator () (const T& a, const T& b, Visitor& v) 
		{
			auto it_a = begin(a);
			auto it_b = begin(b);

			std::string idx_string;
			size_t idx = 0;

			if (v.enterPackContainer(a)) {
				for (; it_a != end(a) && it_b != end(b); it_a++, it_b++)
				{
					v.enter(*it_a, idx_string = std::to_string(idx++));
					VisitCompound<std::remove_cvref_t<decltype(*it_a)>>{}(*it_a, *it_b, v);
					v.leave();
				}
			} else
				v(a, b);
			v.leavePack();

			if (it_a != end(a) || it_b != end(b))
				throw std::runtime_error("visit compound container of unequal size");
		}
	};

	template<typename T>
	struct VisitCompound<T, std::enable_if_t<boost::hana::Struct<T>::value>>
	{
		template<CompoundAssignmentVisitor Visitor>
		void operator () (T& a, const T& b, Visitor& v)
		{
			if (v.enterPackStruct(a))
				boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto member) {
					auto& suba = boost::hana::second(member)(a);
					auto& subb = boost::hana::second(member)(b);

					v.enter(suba, boost::hana::first(member).c_str());
					VisitCompound<std::remove_cvref_t<decltype(suba)>>{}(suba, subb, v);
					v.leave();
				});
			else
				v(a, b);
			v.leavePack();
		}

		template<CompoundUnaryVisitor Visitor>
		void operator () (T& a, Visitor& v)
		{
			if (v.enterPackStruct(a))
				boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto member) {
					auto& suba = boost::hana::second(member)(a);
					if constexpr (Signal<decltype(suba)>)
					{
						v.enter(suba, boost::hana::first(member).c_str());
						VisitCompound<std::remove_cvref_t<decltype(suba)>>{}(suba, v);
						v.leave();
					}
				});
			else
				v(a);
			v.leavePack();
		}

		template<CompoundBinaryVisitor Visitor>
		void operator () (const T& a, const T& b, Visitor& v)
		{
			if (v.enterPackStruct(a)) 
				boost::hana::for_each(boost::hana::accessors<std::remove_cvref_t<T>>(), [&](auto member) {
					auto& suba = boost::hana::second(member)(a);
					auto& subb = boost::hana::second(member)(b);

					v.enter(suba, boost::hana::first(member).c_str());
					VisitCompound<std::remove_cvref_t<decltype(suba)>>{}(suba, subb, v);
					v.leave();
				});
			else
				v(a, b);
			v.leavePack();
		}
	};

	template<typename T>
	struct VisitCompound<std::optional<T>>
	{
		template<CompoundAssignmentVisitor Visitor>
		void operator () (std::optional<T>& a, const std::optional<T>& b, Visitor& v)
		{
			if (b && !a)
				a.emplace(constructFrom(b));

			if (b)
				VisitCompound<std::remove_cvref_t<T>>{}(*a, *b, v);
		}

		template<CompoundUnaryVisitor Visitor>
		void operator () (std::optional<T>& a, Visitor& v)
		{
			if (a)
				VisitCompound<std::remove_cvref_t<T>>{}(*a, v);
		}

		template<CompoundBinaryVisitor Visitor>
		void operator () (const std::optional<T>& a, const std::optional<T>& b, Visitor& v)
		{
			if(a && b)
				VisitCompound<std::remove_cvref_t<T>>{}(*a, *b, v);
		}
	};

	template<typename Comp> requires(!std::is_const_v<Comp>)
	void setName(Comp& compound, std::string_view prefix)
	{
		struct NameVisitor : public CompoundNameVisitor
		{
			void elementaryOnly(ElementarySignal& vec) override { vec.setName(makeName()); }
		};

		static_assert(CompoundUnaryVisitor<NameVisitor>);

		NameVisitor v;
		reccurseCompoundMembers(compound, v, prefix);
	}

	template<typename Comp>
	void setName(const Comp& compound, std::string_view prefix)
	{
		struct NameVisitor : public CompoundNameVisitor
		{
			void elementaryOnly(const ElementarySignal& vec, const ElementarySignal& ) override { vec.setName(makeName()); }
		};

		static_assert(CompoundBinaryVisitor<NameVisitor>);

		NameVisitor v;
		reccurseCompoundMembers(compound, compound, v, prefix);
	}

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

		// cannot transform Reverse signals. (Signals with upstream components) Use downstream() to extract the downstream part.
		template<ReverseSignal T, typename TFunc>
		T transformSignal(const T& val, TFunc&& func) = delete;

		// cannot transform Reverse signals. (Signals with upstream components) Use downstream() to extract the downstream part.
		template<ReverseSignal T, ReverseSignal Tr, typename TFunc>
		T transformSignal(const T& val, const Tr& resetVal, TFunc&& func) = delete;

		template<typename T, typename TFunc>
		T transformIfSignal(const T& signal, TFunc&& func)
		{
			if constexpr(Signal<T>)
				return func(signal);
			else
				return signal;
		}

		template<typename T, typename Tr, typename TFunc>
		T transformIfSignal(const T& signal, Tr&& secondSignal, TFunc&& func)
		{
			if constexpr(Signal<T>)
				return func(signal, secondSignal);
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
						return func(member);
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
						return func(std::get<0>(member), std::get<1>(member));
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
				ret.insert(ret.end(), func(it));

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
				ret.insert(ret.end(), func(it, *it_reset++));

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
				return T{ func(args)... };
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

			static_assert(std::tuple_size<decltype(in2)>::value == std::tuple_size<decltype(in2r)>::value, "Internal gatery error, something got lost upon transforming signals");

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

			static_assert(std::tuple_size<decltype(in2)>::value == std::tuple_size<decltype(in3)>::value, "Internal gatery error, something got lost upon transforming signals");


			static_assert(std::is_same_v<decltype(boost::hana::at_c<0>(in3)), decltype(std::get<0>(in3))>);

			// Essentially return Tuple{ transform(in3[0][0], in3[0][1]), transform(in3[1][0], in3[1][1]), ...}
			return boost::hana::unpack(in3, [&](auto&&... args) {
				return T{ func(std::get<0>(args), std::get<1>(args))...};
			});
		}
	}

	BitWidth width(const BaseSignal auto& signal) { return signal.width(); }

	BitWidth width(const Signal auto& ...args)
	{
		BitWidth sum;

		(internal::transformSignal(args, [&](const auto& arg) {
			if constexpr (Signal<decltype(arg)>)
				sum += width(arg);
			return arg;
		}), ...);

		return sum;
	}
}


/*

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
*/
