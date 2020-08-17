#include "Constant.h"

#include "BitVector.h"

#include <hcl/hlim/coreNodes/Node_Constant.h>

#include <optional>
#include <boost/spirit/home/x3.hpp>

using namespace hcl::core;

sim::DefaultBitVectorState hcl::core::frontend::parseBit(char value)
{
    HCL_DESIGNCHECK(value == '0' || value == '1' || value == 'x' || value == 'X');

    sim::DefaultBitVectorState ret;
    ret.resize(1);
    ret.set(sim::DefaultConfig::VALUE, 0, value != '0');
    ret.set(sim::DefaultConfig::DEFINED, 0, value != 'x' && value != 'X');
    return ret;
}

sim::DefaultBitVectorState hcl::core::frontend::parseBit(bool value)
{
    return parseBit(value ? '1' : '0');
}

sim::DefaultBitVectorState hcl::core::frontend::parseBVec(std::string_view value)
{
    using namespace boost::spirit::x3;

    sim::DefaultBitVectorState ret;

    auto parseWidth = [&](auto& ctx) {
        if (_attr(ctx))
        {
            const size_t width = *_attr(ctx);
            ret.resize(width);
            ret.setRange(sim::DefaultConfig::VALUE, 0, width, false);
            ret.setRange(sim::DefaultConfig::DEFINED, 0, width, true);
        }
    };

    auto parseHex = [&](size_t bps, auto& ctx) {

        std::string_view num = _attr(ctx);
        if (ret.size() == 0)
        {
            ret.resize(num.size() * bps);
        }
        else
            HCL_DESIGNCHECK_HINT(ret.size() >= num.size() * bps, "string BVec constant width is to small for its value");

        for (size_t i = 0; i < num.size(); ++i)
        {
            size_t value = 0;
            size_t defined = 0xFF;
            if (num[i] >= '0' && num[i] <= '9')
                value = num[i] - '0';
            else if (num[i] >= 'a' && num[i] <= 'f')
                value = num[i] - 'a';
            else if (num[i] >= 'A' && num[i] <= 'F')
                value = num[i] - 'A';
            else
                defined = 0;

            size_t dstIdx = num.size() - 1 - i;
            ret.insertNonStraddling(sim::DefaultConfig::VALUE, dstIdx * bps, bps, value);
            ret.insertNonStraddling(sim::DefaultConfig::DEFINED, dstIdx * bps, bps, defined);
        }
    };

    try {
        parse(value.begin(), value.end(),
            (-uint_)[parseWidth] > (
                (char_('x') > (*char_("0-9a-fA-FxX"))[std::bind(parseHex, 4, std::placeholders::_1)]) |
                (char_('o') > (*char_("0-7xX"))[std::bind(parseHex, 3, std::placeholders::_1)]) |
                (char_('b') > (*char_("0-1xX"))[std::bind(parseHex, 1, std::placeholders::_1)])
                ) > eoi
        );
    }
    catch (const expectation_failure<std::string_view::iterator>& err)
    {
        HCL_DESIGNCHECK_HINT(false, "parsing of BVec literal failed (32xF, b0, ...)");
    }

    return ret;
}

sim::DefaultBitVectorState hcl::core::frontend::parseBVec(uint64_t value, size_t width)
{
    HCL_ASSERT(width <= sizeof(size_t) * 8);

    sim::DefaultBitVectorState ret;
    ret.resize(width);
    ret.insertNonStraddling(sim::DefaultConfig::VALUE, 0, width, value);
    ret.setRange(sim::DefaultConfig::DEFINED, 0, width, true);
    return ret;
}

sim::DefaultBitVectorState hcl::core::frontend::undefinedBVec(size_t width)
{
    sim::DefaultBitVectorState ret;
    ret.resize(width);
    ret.setRange(sim::DefaultConfig::DEFINED, 0, width, false);
    return ret;
}


hcl::core::frontend::BVec hcl::core::frontend::ConstBVec(uint64_t value, size_t width)
{
    auto* node = DesignScope::createNode<hlim::Node_Constant>(parseBVec(value, width), hlim::ConnectionType::BITVEC);
    return SignalReadPort(node);
}
