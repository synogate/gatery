#include "OneHot.h"

using namespace hcl::core::frontend;

hcl::stl::OneHot hcl::stl::decoder(const BVec& in)
{
	OneHot ret(1ull << in.size());

	for (size_t i = 0; i < ret.size(); ++i)
		ret[i] = in == i;

	return ret;
}

BVec hcl::stl::encoder(const OneHot& in)
{
	BVec ret(utils::Log2C(in.size()), Expansion::none);

	ret = 0;
	for (size_t i = 0; i < in.size(); ++i)
		ret |= zext(i & in[i]);

	return ret;
}

BVec hcl::stl::priorityEncoder(const BVec& in)
{
	BVec ret(utils::Log2C(in.size()), Expansion::none);

	for (size_t i = in.size() - 1; i < in.size(); --i)
		IF(in[i])
			ret = i;

	return ret;
}
