#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
	class MemoryMap
	{
	public:
		struct RegDesc
		{
			std::string name;
			std::string desc;
		};

		virtual void ro(const BVec& value, RegDesc desc) {}
		virtual void ro(const Bit& value, RegDesc desc) {}
		virtual Bit rw(BVec& value, RegDesc desc) { return Bit{}; }
		virtual Bit rw(Bit& value, RegDesc desc) { return Bit{}; }
		virtual Bit wo(BVec& value, RegDesc desc) { return rw(value, desc); }
		virtual Bit wo(Bit& value, RegDesc desc) { return rw(value, desc); }
	};
}
