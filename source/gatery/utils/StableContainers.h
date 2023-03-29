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

#include <set>
#include <map>
#include <concepts>
#include <type_traits>

namespace gtry::utils {

/**
 * @brief Yields a stable < comparison operator for std::set or std::map that must be implemented explicitly for all types passed as keys to StableSet or StableMap.
 * @details Stable means that the outcome is the same even if memory addresses change between runs. 
 */
template<typename Type>
struct StableCompare
{
	// Implement this in specializations for all classes and data types which are to be used as keys in stable containers
	// bool operator()(const Type &lhs, const Type &rhs) const;
};


/**
 * @brief If type is a pointer to a const, strips the const.
 */
template<typename Type, typename En = void>
struct ConstFreePointer { using type = Type; };

template<typename Pointer>
struct ConstFreePointer<Pointer, std::enable_if_t<std::is_pointer<Pointer>::value>> { 
	using type = typename std::add_pointer<
				typename std::remove_const<
					typename std::remove_pointer<Pointer>::type
				>::type
	>::type; 
};



/// A std::set that is stable wrt. changing memory addresses by forcing the implementation of a stable comparison function for key types.
template<typename Type>
using StableSet = std::set<Type, StableCompare<typename ConstFreePointer<Type>::type>>;

/// A std::map that is stable wrt. changing memory addresses by forcing the implementation of a stable comparison function for key types.
template<typename KeyType, typename ValueType>
using StableMap = std::map<KeyType, ValueType, StableCompare<typename ConstFreePointer<KeyType>::type>>;

// An unstable std::set, but deprived of the ability to iterate over the contained elements thus making it stable to use.
template<typename Type>
class UnstableSet
{
	public:
		UnstableSet() = default;
		
		template<typename Iterator>
		UnstableSet(Iterator begin, Iterator end) : m_set(begin, end) { }

		void insert(const Type &elem) { m_set.insert(elem); }
		void erase(const Type &elem) { m_set.erase(elem); }
		bool contains(const Type &elem) { return m_set.contains(elem); }
	protected:
		std::set<Type> m_set;
};


}

///////////////////////////////////


namespace gtry::hlim {
	struct NodePort;
	struct RefCtdNodePort;

	class Clock;
	class BaseNode;
	class Node_Pin;
}

namespace gtry::utils {

template<>
struct StableCompare<hlim::NodePort>
{
	bool operator()(const hlim::NodePort &lhs, const hlim::NodePort &rhs) const;
};

template<>
struct StableCompare<hlim::RefCtdNodePort>
{
	bool operator()(const hlim::RefCtdNodePort &lhs, const hlim::RefCtdNodePort &rhs) const;
};

template<>
struct StableCompare<hlim::Clock*>
{
	bool operator()(const hlim::Clock* const &lhs, const hlim::Clock* const &rhs) const;
};

bool stableCompareNodes(const hlim::BaseNode* const &lhs, const hlim::BaseNode* const &rhs);

template<std::derived_from<hlim::BaseNode> NodeType>
struct StableCompare<NodeType*>
{
	bool operator()(const NodeType* const &lhs, const NodeType* const &rhs) const {
		return stableCompareNodes(lhs, rhs);
	}
};


// For forward declared stuff

template<>
struct StableCompare<hlim::Node_Pin*>
{
	bool operator()(const hlim::Node_Pin* const &lhs, const hlim::Node_Pin* const &rhs) const;
};

}

