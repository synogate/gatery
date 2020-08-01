#include "SignalPort.h"
#include "Constant.h"

using namespace hcl;

hcl::core::frontend::BitSignalPort::BitSignalPort(char bit) :
	BitSignalPort(ConstBit(bit))
{
}

hcl::core::frontend::BitSignalPort::BitSignalPort(bool bit) :
	BitSignalPort(ConstBit(bit))
{
}

hcl::core::frontend::BitSignalPort::BitSignalPort(const Bit& bit)
{
	setPort(bit.getReadPort());
	setName(bit.getName());
}

hcl::core::frontend::BitSignalPort::BitSignalPort(const BVecBitProxy<BVec>& bit) :
	BitSignalPort(Bit{ bit })
{
}

hcl::core::frontend::BVecSignalPort::BVecSignalPort(std::string_view vec) :
	BVecSignalPort(ConstBVec(vec))
{
}

hcl::core::frontend::BVecSignalPort::BVecSignalPort(const BVec& vec)
{
	setPort(vec.getReadPort());
	setName(vec.getName());
}

hcl::core::frontend::BVecSignalPort::BVecSignalPort(const BVecSlice& vec) :
	BVecSignalPort(BVec{vec})
{
}
