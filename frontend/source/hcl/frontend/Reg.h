#pragma once

namespace hcl::core::frontend
{
	template<class T, class En = void>
	struct Reg
	{
		T operator () (const T& val) { return val; }
	};

	template<typename T>
	T reg(const T& val) { return Reg<T>{}(val); }

	template<typename T, typename Tr>
	T reg(const T& val, const Tr& resetVal) { return Reg<T>{}(val, resetVal); }
}
