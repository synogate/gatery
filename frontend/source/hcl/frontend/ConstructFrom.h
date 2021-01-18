#pragma once
#include "BitVector.h"
#include "Bit.h"

#include <boost/spirit/home/support/container.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/fwd/accessors.hpp>

namespace hcl::core::frontend 
{
	namespace internal
	{
		template <typename T, typename = int>
		struct resizable : std::false_type {};

		template <typename T>
		struct resizable <T, decltype((void)std::declval<T>().resize(1), 0)> : std::true_type {};
	}

	template<typename T>
	void constructFrom(const T& src, T& dst)
	{
		if constexpr (boost::spirit::traits::is_container<T>::value)
		{
			if constexpr (internal::resizable<T>::value)
				dst.resize(src.size());

			auto it_src = begin(src);
			for (auto& d : dst)
				constructFrom(*it_src++, d);
		}
		else if constexpr (boost::hana::Struct<T>::value)
		{
			boost::hana::for_each(boost::hana::accessors<std::remove_cv_t<T>>(), [&](auto member) {
				constructFrom(boost::hana::second(member)(src), boost::hana::second(member)(dst));
			});
		}
		else if constexpr (std::is_base_of_v<BVec, T>)
		{
			dst = src.getWidth();
		}
		else if constexpr (std::is_base_of_v<Bit, T>)
		{
			// nothing todo
		}
		else
		{
			// copy meta data
			dst = src;
		}
	}

	template<typename T>
	T constructFrom(const T& src)
	{
		T ret;
		constructFrom(src, ret);
		return ret;
	}


};
