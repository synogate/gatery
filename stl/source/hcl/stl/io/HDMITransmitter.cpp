#include "HDMITransmitter.h"

#include "../utils/BitCount.h"

namespace hcl::stl::hdmi {

using namespace hcl::core::frontend;
using namespace hcl::core::hlim;


core::frontend::BitVector tmdsEncode(core::hlim::BaseClock *pixelClock, core::frontend::Bit sendData, core::frontend::UnsignedInteger data, core::frontend::BitVector ctrl)
{
    HCL_NAMED(sendData);
    HCL_NAMED(data);
    HCL_NAMED(ctrl);
    
    GroupScope entity(NodeGroup::GRP_ENTITY);
    entity
        .setName("tmdsEncode")
        .setComment("Encodes 8-bit data words to 10-bit TMDS words with control bits");
        
    
    RegisterFactory reg({.clk = pixelClock});

    HCL_DESIGNCHECK_HINT(data.getWidth() == 8, "data must be 8 bit wide");
    HCL_DESIGNCHECK_HINT(ctrl.getWidth() == 2, "data must be 8 bit wide");
    
    HCL_COMMENT << "Count the number of high bits in the input word";
    UnsignedInteger sumOfOnes = bitcount(data);
    HCL_NAMED(sumOfOnes);   

    HCL_COMMENT << "Prepare XORed and XNORed data words to select from based on number of high bits";

    UnsignedInteger dataXNOR = data;
    UnsignedInteger dataXOR = data;
    for (auto i : utils::Range<size_t>(1, data.getWidth())) {
        dataXOR.setBit(i, data[i] ^ dataXOR[i-1]);
        dataXNOR.setBit(i, data[i] == dataXNOR[i-1]);
    }

    HCL_NAMED(dataXNOR);
    HCL_NAMED(dataXOR);
    
    SignedInteger disparity(4);
    HCL_NAMED(disparity);
    SignedInteger newDisparity;
    HCL_NAMED(newDisparity);
    
    core::frontend::BitVector result(10);
    HCL_NAMED(result);
    
    HCL_COMMENT << "If sending data, 8/10 encode the data, otherwise encode the control bits";
    IF (sendData) {
    } ELSE {
        PriorityConditional<core::frontend::BitVector> con;
        
        con
            .addCondition(ctrl == 0b00_vec, 0b1101010100_vec)
            .addCondition(ctrl == 0b01_vec, 0b0010101011_vec)
            .addCondition(ctrl == 0b10_vec, 0b0101010100_vec)
            .addCondition(ctrl == 0b11_vec, 0b1010101011_vec);
            
        result = con(0b0000000000_vec);
        
        newDisparity = 0b0000_svec;
    }

    HCL_COMMENT << "Keep a running (signed) counter of the imbalance on the line, to modify future data encodings accordingly";
    driveWith(disparity, reg(newDisparity, true, 0b0000_svec));
    
    return result;
}

core::frontend::BitVector tmdsEncodeReduceTransitions(const core::frontend::BitVector& data)
{
    HCL_COMMENT << "Count the number of high bits in the input word";
    UnsignedInteger sumOfOnes = bitcount(data);
    HCL_NAMED(sumOfOnes);

    HCL_COMMENT << "Prepare XORed and XNORed data words to select from based on number of high bits";

    // TODO: rename getWidth to size to be more container like
    // TODO: allow compare of different bit width
    UnsignedInteger literalConstant = (4_uvec).ext(sumOfOnes.getWidth());
    Bit invert = sumOfOnes > literalConstant || (sumOfOnes == literalConstant && data[0] == false);

    BitVector tmdsReduced{ data.getWidth() + 1 };
    HCL_NAMED(tmdsReduced);

    tmdsReduced.setBit(0, data[0]);
    for (auto i : utils::Range<size_t>(1, data.getWidth())) {
        tmdsReduced.setBit(i, data[i] ^ tmdsReduced[i - 1] ^ invert);
    }

    HCL_COMMENT << "Decode using 1=xor, 0=xnor";
    tmdsReduced.setBit(data.getWidth(), ~invert);

    return tmdsReduced;
}

core::frontend::BitVector tmdsDecodeReduceTransitions(const core::frontend::BitVector& data)
{
    BitVector decoded = data.zext(data.getWidth() - 1);
    decoded = decoded ^ (decoded << 1); // TODO: ^ invert operator missing
    HCL_NAMED(decoded);

    IF(!data[data.getWidth() - 1])
        decoded(1, decoded.getWidth()-1) = ~(BitVector)decoded(1, decoded.getWidth() - 1);

    return decoded;
}

core::frontend::BitVector tmdsEncodeBitflip(const core::frontend::RegisterFactory& clk, const core::frontend::BitVector& data)
{
    HCL_COMMENT << "count the number of uncompensated ones";
    Register<UnsignedInteger> global_counter{ clk.config(), 0b0000_uvec };
    HCL_NAMED(global_counter);

    // TODO: depend with and start value on data width
    UnsignedInteger word_counter = 0b1011_uvec; // -9 truncated
    for (size_t i = 0; i < data.getWidth(); ++i)
    {
        UnsignedInteger tmp{ 1 };
        tmp.setBit(0, data[i]);
        word_counter += tmp;
    }

    Bit invert = word_counter[word_counter.getWidth() - 1] == global_counter.delay(1)[global_counter.getWidth() - 1];
    HCL_NAMED(invert);

    BitVector result = cat(invert, data); // TODO: data ^ invert
    HCL_NAMED(result);

    IF(invert)
    {
        // TODO: add sub/add alu
        global_counter = global_counter.delay(1) - word_counter; // TODO: initialize registers with its own delay value
        result = cat(1_vec, ~data);
    }
    ELSE
    {
        global_counter = global_counter.delay(1) + word_counter;
    }

    return result;
}

core::frontend::BitVector hdmi::tmdsDecodeBitflip(const core::frontend::BitVector& data)
{
    // TODO: should be return data(0, -1) ^ data[back];
    BitVector tmp = data.zext(data.getWidth()-1);
    HCL_NAMED(tmp);
    for (size_t i = 0; i < tmp.getWidth(); ++i)
        tmp.setBit(i, tmp[i] ^ data[data.getWidth() - 1]);
    return tmp;
}

TmdsEncoder::TmdsEncoder(core::frontend::RegisterFactory& clk) :
    m_Clk{clk}
{
    m_Channel.fill(0b0010101011_vec); // no data symbol
    m_Channel[0].setName("redChannel"); // TODO: convinient method to name arrays
    m_Channel[0].setName("greenChannel");
    m_Channel[0].setName("blueChannel");
}

void TmdsEncoder::addSync(const core::frontend::Bit& hsync, const core::frontend::Bit& vsync)
{
    IF(hsync)
        setSync(true, false);
    IF(vsync)
        setSync(false, true);
    IF(hsync && vsync)
        setSync(true, true);
}

void hcl::stl::hdmi::TmdsEncoder::setColor(const ColorRGB& color)
{
    m_Channel[0] = tmdsEncodeBitflip(m_Clk, tmdsEncodeReduceTransitions(color.r));
    m_Channel[1] = tmdsEncodeBitflip(m_Clk, tmdsEncodeReduceTransitions(color.g));
    m_Channel[2] = tmdsEncodeBitflip(m_Clk, tmdsEncodeReduceTransitions(color.b));
}

void hcl::stl::hdmi::TmdsEncoder::setSync(bool hsync, bool vsync)
{
    if (hsync && vsync)
        m_Channel[2] = 0b1010101011_vec;
    else if (hsync)
        m_Channel[2] = 0b0010101011_vec;
    else if (vsync)
        m_Channel[2] = 0b0101010100_vec;
    else
        m_Channel[2] = 0b0010101011_vec;
}

void hcl::stl::hdmi::TmdsEncoder::setTERC4(const core::frontend::BitVector& ctrl)
{
    const BitVector trec4lookup[] = {
        0b1010011100_vec,
        0b1001100011_vec,
        0b1011100100_vec,
        0b1011100010_vec,
        0b0101110001_vec,
        0b0100011110_vec,
        0b0110001110_vec,
        0b0100111100_vec,
        0b1011001100_vec,
        0b0100111001_vec,
        0b0110011100_vec,
        0b1011000110_vec,
        0b1010001110_vec,
        0b1001110001_vec,
        0b0101100011_vec,
        0b1011000011_vec
    };

    HCL_ASSERT(ctrl.getWidth() == 6);
    //m_Channel[0] = mux(ctrl(0, 2), trec4lookup); // TODO: improve mux to accept any container as second input
    //m_Channel[1] = mux(ctrl(2, 2), trec4lookup); // TODO: subrange as argument for mux
    //m_Channel[2] = mux(ctrl(4, 2), trec4lookup);
}

SerialTMDS hcl::stl::hdmi::TmdsEncoder::serialOutput() const
{
    // TODO: use shift register/serdes lib for automatic vendor specific serdes usage

    // TODO: create multiplied clock from m_Clk
    RegisterFactory& fastClk = m_Clk;

    Register<BitVector> chan[] = { 
        {fastClk.config(), 0b0000000000_vec}, // TODO: Register<> without reset
        {fastClk.config(), 0b0000000000_vec},
        {fastClk.config(), 0b0000000000_vec}
    };

    for (auto& c : chan)
        c = c.delay(1) >> 1;

    Register<UnsignedInteger> shiftCounter{ fastClk.config(), 0x0_uvec };
    HCL_NAMED(shiftCounter);
    shiftCounter = shiftCounter.delay(1) + 1_uvec;

    IF(shiftCounter == 9_uvec)
    {
        shiftCounter = 0x0_uvec;

        for (size_t i = 0; i < m_Channel.size(); ++i)
            chan[i] = m_Channel[i]; // TODO: clock domain crossing lib and warning
    }

    SerialTMDS out;
    // TODO: convinient method to name signals in structs

    out.clock.pos = true; // TODO: signal from clock
    out.clock.neg = false;

    for (size_t i = 0; i < m_Channel.size(); ++i)
    {
        out.data[i].pos = chan[i].lsb();
        out.data[i].neg = ~chan[i].lsb();
    }

    return SerialTMDS();
}

}
