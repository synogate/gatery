#include "HDMITransmitter.h"

#include "../utils/BitCount.h"

namespace hcl::stl::hdmi {

using namespace hcl::core::frontend;


core::frontend::BVec tmdsEncode(core::frontend::Clock &pixelClock, core::frontend::Bit dataEnable, core::frontend::BVec data, core::frontend::BVec ctrl)
{
    HCL_NAMED(dataEnable);
    HCL_NAMED(data);
    HCL_NAMED(ctrl);
    
    GroupScope entity(GroupScope::GroupType::ENTITY);
    entity
        .setName("tmdsEncode")
        .setComment("Encodes 8-bit data words to 10-bit TMDS words with control bits");
        

    HCL_DESIGNCHECK_HINT(data.getWidth() == 8, "data must be 8 bit wide");
    HCL_DESIGNCHECK_HINT(ctrl.getWidth() == 2, "data must be 8 bit wide");
    
    HCL_COMMENT << "Count the number of high bits in the input word";
    BVec sumOfOnes_data = bitcount(data);
    HCL_NAMED(sumOfOnes_data);   

    HCL_COMMENT << "Prepare XORed and XNORed data words to select from based on number of high bits";

    BVec dataXNOR = data;
    BVec dataXOR = data;
    for (auto i : utils::Range<size_t>(1, data.getWidth())) {
        dataXOR.setBit(i, data[i] ^ dataXOR[i-1]);
        dataXNOR.setBit(i, data[i] == dataXNOR[i-1]);
    }

    HCL_NAMED(dataXNOR);
    HCL_NAMED(dataXOR);
    
    Bit useXnor = !((sumOfOnes_data > 4_bvec) | ((sumOfOnes_data == 4_bvec) & (!data[0])));
    HCL_NAMED(useXnor);
    BVec q_m = dataXOR;
    HCL_NAMED(q_m);
    IF (useXnor)
        q_m = dataXNOR;
    
    HCL_COMMENT << "Keep a running (signed) counter of the imbalance on the line, to modify future data encodings accordingly";
    Register<BVec> imbalance;
    imbalance.setReset(0b0000_bvec);
    imbalance.setClock(pixelClock);
    HCL_NAMED(imbalance);
    
    core::frontend::BVec result(10);
    HCL_NAMED(result);
    
    HCL_COMMENT << "If sending data, 8/10 encode the data, otherwise encode the control bits";
    IF (dataEnable) {
        
        HCL_COMMENT << "Count the number of high bits in the xor/xnor word";
        BVec sumOfOnes_q_m = bitcount(q_m);
        HCL_NAMED(sumOfOnes_q_m);   
        
        Bit noPreviousImbalance = imbalance.delay(1) == 0_bvec;
        HCL_NAMED(noPreviousImbalance);
        Bit noImbalanceInQ_m = sumOfOnes_q_m == 4_bvec;
        HCL_NAMED(noImbalanceInQ_m);
        
        IF (noPreviousImbalance | noImbalanceInQ_m) {
            result(0, 8) = mux(useXnor, {q_m, ~q_m});
            result.setBit(8, useXnor);
            result.setBit(9, ~useXnor);
            
            IF (useXnor) 
                imbalance = imbalance.delay(1) - 8_bvec + sumOfOnes_q_m + sumOfOnes_q_m;
            ELSE
                imbalance = imbalance.delay(1) + 8_bvec - sumOfOnes_q_m - sumOfOnes_q_m;
            
        } ELSE {
            Bit positivePreviousImbalance = !imbalance.delay(1).msb(); // Sign bit
            HCL_NAMED(positivePreviousImbalance);
            Bit positiveImbalanceInQ_m = sumOfOnes_q_m > 4_bvec;
            HCL_NAMED(positiveImbalanceInQ_m);
            IF ((positivePreviousImbalance & positiveImbalanceInQ_m) |
                ((!positivePreviousImbalance) & (!positiveImbalanceInQ_m))) {
                
                result(0, 8) = ~q_m;
                result.setBit(8, useXnor);
                result.setBit(9, true);
                
                imbalance = imbalance.delay(1) + 8_bvec - sumOfOnes_q_m - sumOfOnes_q_m;
                IF (useXnor)
                    imbalance = (BVec) imbalance + 2_bvec;
            } ELSE {
                result(0, 8) = q_m;
                result.setBit(8, useXnor);
                result.setBit(9, true);
                
                imbalance = imbalance.delay(1) + 8_bvec - sumOfOnes_q_m - sumOfOnes_q_m;
                IF (useXnor)
                    imbalance = (BVec) imbalance + 2_bvec;
            }
        }
    } ELSE {
        PriorityConditional<core::frontend::BVec> con;
        
        con
            .addCondition(ctrl == 0b00_bvec, 0b1101010100_bvec)
            .addCondition(ctrl == 0b01_bvec, 0b0010101011_bvec)
            .addCondition(ctrl == 0b10_bvec, 0b0101010100_bvec)
            .addCondition(ctrl == 0b11_bvec, 0b1010101011_bvec);
            
        result = con(0b0000000000_bvec);
        
        imbalance = 0b0000_bvec;
    }

    return result;
}

core::frontend::BVec tmdsEncodeReduceTransitions(const core::frontend::BVec& data)
{
    HCL_COMMENT << "Count the number of high bits in the input word";
    BVec sumOfOnes = bitcount(data);
    HCL_NAMED(sumOfOnes);

    HCL_COMMENT << "Prepare XORed and XNORed data words to select from based on number of high bits";

    // TODO: rename getWidth to size to be more container like
    // TODO: allow compare of different bit width
    BVec literalConstant = (4_bvec).zext(sumOfOnes.getWidth());
    Bit invert = sumOfOnes > literalConstant || (sumOfOnes == literalConstant && data[0] == false);

    BVec tmdsReduced{ data.getWidth() + 1 };
    HCL_NAMED(tmdsReduced);

    tmdsReduced.setBit(0, data[0]);
    for (auto i : utils::Range<size_t>(1, data.getWidth())) {
        tmdsReduced.setBit(i, data[i] ^ tmdsReduced[i - 1] ^ invert);
    }

    HCL_COMMENT << "Decode using 1=xor, 0=xnor";
    tmdsReduced.setBit(data.getWidth(), ~invert);

    return tmdsReduced;
}

core::frontend::BVec tmdsDecodeReduceTransitions(const core::frontend::BVec& data)
{
    BVec decoded = data.zext(data.getWidth() - 1);
    decoded = decoded ^ (decoded << 1); // TODO: ^ invert operator missing
    HCL_NAMED(decoded);

    IF(!data[data.getWidth() - 1])
        decoded(1, decoded.getWidth()-1) = ~(BVec)decoded(1, decoded.getWidth() - 1);

    return decoded;
}

core::frontend::BVec tmdsEncodeBitflip(const core::frontend::Clock& clk, const core::frontend::BVec& data)
{
    HCL_COMMENT << "count the number of uncompensated ones";
    Register<BVec> global_counter{3ull};
    global_counter.setClock(clk);
    global_counter.setReset(0b000_bvec);
    HCL_NAMED(global_counter);

    // TODO: depend with and start value on data width
    BVec word_counter = 0b100_bvec;
    for (size_t i = 0; i < data.getWidth(); ++i)
    {
        BVec tmp{ 1 };
        tmp.setBit(0, data[i]);
        word_counter += tmp;
    }

    Bit invert = word_counter[word_counter.getWidth() - 1] == global_counter.delay(1)[global_counter.getWidth() - 1];
    HCL_NAMED(invert);

    BVec result = cat(invert, data); // TODO: data ^ invert
    HCL_NAMED(result);

    IF(invert)
    {
        // TODO: add sub/add alu
        global_counter = global_counter.delay(1) - word_counter; // TODO: initialize registers with its own delay value
        result = cat(1_bvec, ~data);
    }
    ELSE
    {
        global_counter = global_counter.delay(1) + word_counter;
    }

    return result;
}

core::frontend::BVec tmdsDecodeBitflip(const core::frontend::BVec& data)
{
    // TODO: should be return data(0, -1) ^ data[back];
    BVec tmp = data.zext(data.getWidth()-1);
    HCL_NAMED(tmp);
    for (size_t i = 0; i < tmp.getWidth(); ++i)
        tmp.setBit(i, tmp[i] ^ data[data.getWidth() - 1]);
    return tmp;
}

TmdsEncoder::TmdsEncoder(core::frontend::Clock& clk) :
    m_Clk{clk}
{
    m_Channel.fill(0b0010101011_bvec); // no data symbol
    m_Channel[0].setName("redChannel"); // TODO: convinient method to name arrays
    m_Channel[1].setName("greenChannel");
    m_Channel[2].setName("blueChannel");
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
        m_Channel[2] = 0b1010101011_bvec;
    else if (hsync)
        m_Channel[2] = 0b0010101011_bvec;
    else if (vsync)
        m_Channel[2] = 0b0101010100_bvec;
    else
        m_Channel[2] = 0b0010101011_bvec;
}

void hcl::stl::hdmi::TmdsEncoder::setTERC4(core::frontend::BVec ctrl)
{
    std::array<BVec, 16> trec4lookup = {
        0b1010011100_bvec,
        0b1001100011_bvec,
        0b1011100100_bvec,
        0b1011100010_bvec,
        0b0101110001_bvec,
        0b0100011110_bvec,
        0b0110001110_bvec,
        0b0100111100_bvec,
        0b1011001100_bvec,
        0b0100111001_bvec,
        0b0110011100_bvec,
        0b1011000110_bvec,
        0b1010001110_bvec,
        0b1001110001_bvec,
        0b0101100011_bvec,
        0b1011000011_bvec
    };

    HCL_ASSERT(ctrl.getWidth() == 6);
    m_Channel[0] = mux(ctrl(0, 2), trec4lookup); // TODO: improve mux to accept any container as second input
    m_Channel[1] = mux(ctrl(2, 2), trec4lookup); // TODO: subrange as argument for mux
    m_Channel[2] = mux(ctrl(4, 2), trec4lookup);
}

SerialTMDS hcl::stl::hdmi::TmdsEncoder::serialOutput() const
{
    // TODO: use shift register/serdes lib for automatic vendor specific serdes usage

    Clock fastClk = m_Clk.deriveClock(ClockConfig{}.setFrequencyMultiplier(10).setName("TmdsEncoderFastClock"));

    Register<BVec> chan[3];
    chan[0].setClock(fastClk);
    chan[1].setClock(fastClk);
    chan[2].setClock(fastClk);
    
    for (auto& c : chan)
        c = c.delay(1) >> 1;

    Register<BVec> shiftCounter(4u);
    shiftCounter.setReset(0x0_bvec);
    shiftCounter.setClock(fastClk);
    HCL_NAMED(shiftCounter);
    shiftCounter = shiftCounter.delay(1) + 1_bvec;

    IF(shiftCounter == 9_bvec)
    {
        shiftCounter = 0x0_bvec;

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
