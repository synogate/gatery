#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
	template<typename TVec = BVec>
	class Adder
	{
	public:
		Adder() = default;
		Adder(const Adder&) = default;

		template<typename TOperand>
		Adder& add(const TOperand& b)
		{
			if (m_count++ == 0)
				m_sum = b;
			else
				m_sum += b;
			return *this;
		}

		template<typename TOperand>
		Adder& operator + (const TOperand& b) { return add(b); }

		template<typename TOperand>
		Adder& operator += (const TOperand& b) { return add(b); }

		const TVec& sum() const { return m_sum; }
		operator const TVec& () const { return m_sum; }

	private:
		size_t m_count = 0;
		TVec m_sum;

	};

	extern template class hcl::stl::Adder<hcl::BVec>;

	class CarrySafeAdder
	{
	public:
		CarrySafeAdder() = default;
		CarrySafeAdder(const CarrySafeAdder&) = default;

		CarrySafeAdder& add(const BVec&);
		CarrySafeAdder operator + (const BVec& b) { CarrySafeAdder ret = *this; ret.add(b); return ret; }
		CarrySafeAdder& operator += (const BVec& b) { return add(b); }

		BVec sum() const;
		operator BVec () const { return sum(); }

	private:
		size_t m_count = 0;
		BVec m_sum;
		BVec m_carry;
	};
}
