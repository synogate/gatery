#pragma once
#include "Compound.h"

namespace hcl::core::frontend 
{
	template<typename T>
	void constructFrom(const T& src, T& dst)
	{
		struct ConstructFromVisitor : CompoundVisitor
		{
			void operator () (BVec& a, const BVec& b) override {
				if (a.size() != b.size())
					a = b.getWidth();
			}
		};

		ConstructFromVisitor v;
		VisitCompound<T>{}(dst, src, v, 0);
	}

	template<typename T>
	T constructFrom(const T& src)
	{
		T ret;
		constructFrom(src, ret);
		return ret;
	}
}
