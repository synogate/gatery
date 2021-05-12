/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "gatery/pch.h"
#include "HDMITransmitter.h"

#include "../utils/BitCount.h"

namespace gtry::scl::hdmi {

using namespace gtry;


BVec tmdsEncode(Clock &pixelClock, Bit dataEnable, BVec data, BVec ctrl)
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

    const size_t subWidth = data.getWidth() - 1;
    BVec dataXNOR = data;
    dataXNOR(1, subWidth) = lxnor(dataXNOR(1, subWidth), dataXNOR(0, subWidth));
    BVec dataXOR = data;
    dataXOR(1, subWidth) ^= dataXOR(0, subWidth);

    HCL_NAMED(dataXNOR);
    HCL_NAMED(dataXOR);
    
    Bit useXnor = !((sumOfOnes_data > 4) | ((sumOfOnes_data == 4) & (!data[0])));
    HCL_NAMED(useXnor);
    BVec q_m = dataXOR;
    HCL_NAMED(q_m);
    IF (useXnor)
        q_m = dataXNOR;
    
    HCL_COMMENT << "Keep a running (signed) counter of the imbalance on the line, to modify future data encodings accordingly";
    Register<BVec> imbalance{ 4_b };
    imbalance.setReset("b0000");
    imbalance.setClock(pixelClock);
    HCL_NAMED(imbalance);
    
    BVec result(10_b, Expansion::none);
    HCL_NAMED(result);
    
    HCL_COMMENT << "If sending data, 8/10 encode the data, otherwise encode the control bits";
    IF (dataEnable) {
        
        HCL_COMMENT << "Count the number of high bits in the xor/xnor word";
        BVec sumOfOnes_q_m = bitcount(q_m);
        HCL_NAMED(sumOfOnes_q_m);   
        
        Bit noPreviousImbalance = imbalance.delay(1) == 0;
        HCL_NAMED(noPreviousImbalance);
        Bit noImbalanceInQ_m = sumOfOnes_q_m == 4;
        HCL_NAMED(noImbalanceInQ_m);
        
        IF (noPreviousImbalance | noImbalanceInQ_m) {
            result(0, 8) = mux(useXnor, {q_m, ~q_m});
            result(8, 2) = pack(useXnor, ~useXnor);
            
            IF (useXnor) 
                imbalance = imbalance.delay(1) - 8 + sumOfOnes_q_m + sumOfOnes_q_m;
            ELSE
                imbalance = imbalance.delay(1) + 8 - sumOfOnes_q_m - sumOfOnes_q_m;
            
        } ELSE {
            Bit positivePreviousImbalance = !imbalance.delay(1).msb(); // Sign bit
            HCL_NAMED(positivePreviousImbalance);
            Bit positiveImbalanceInQ_m = sumOfOnes_q_m > 4;
            HCL_NAMED(positiveImbalanceInQ_m);
            IF ((positivePreviousImbalance & positiveImbalanceInQ_m) |
                ((!positivePreviousImbalance) & (!positiveImbalanceInQ_m))) {
                
                result(0, 8) = ~q_m;
                result(8, 2) = pack(useXnor, '1');
                
                imbalance = imbalance.delay(1) + 8 - sumOfOnes_q_m - sumOfOnes_q_m;
                IF (useXnor)
                    imbalance = (BVec) imbalance + 2;
            } ELSE {
                result(0, 8) = q_m;
                result(8, 2) = pack(useXnor, '1');
                
                imbalance = imbalance.delay(1) + 8 - sumOfOnes_q_m - sumOfOnes_q_m;
                IF (useXnor)
                    imbalance = (BVec) imbalance + 2;
            }
        }
    } ELSE {
        PriorityConditional<BVec> con;
        
        con
            .addCondition(ctrl == "b00", "b1101010100")
            .addCondition(ctrl == "b01", "b0010101011")
            .addCondition(ctrl == "b10", "b0101010100")
            .addCondition(ctrl == "b11", "b1010101011");
            
        result = con("b0000000000");
        
        imbalance = "b0000";
    }

    return result;
}

BVec tmdsEncodeSymbol(const BVec& data)
{
    GroupScope ent{ GroupScope::GroupType::ENTITY };
    ent.setName("tmdsEncodeSymbol");

    BVec sumOfOnes = bitcount(data);
    HCL_NAMED(sumOfOnes);

    // minimize number of transitions
    Bit invertXor = (sumOfOnes > 4u) | (sumOfOnes == 4u & !data.lsb());
    HCL_NAMED(invertXor);

    HCL_COMMENT << "Decode using 1=xor, 0=xnor";
    BVec transitionReduced = data;
    for (auto i : utils::Range<size_t>(1, transitionReduced.size()))
        transitionReduced[i] ^= transitionReduced[i - 1] ^ invertXor;
    HCL_NAMED(transitionReduced);

    // even out 0 and 1 bits
    BVec word_counter = zext(sumOfOnes, 1) - zext(data.size() / 2);
    HCL_NAMED(word_counter);
    BVec global_counter = word_counter.getWidth();
    HCL_NAMED(global_counter);

    Bit invert = word_counter.msb() == global_counter.msb();
    HCL_NAMED(invert);

    // sub or add depending on invert
    global_counter += (word_counter ^ invert) + invert;
    global_counter = reg(global_counter, 0);

    BVec result = pack(invert, ~invertXor, transitionReduced ^ invert);
    HCL_NAMED(result);
    return result;
}

BVec tmdsEncodeReduceTransitions(const BVec& data)
{
    HCL_COMMENT << "Count the number of high bits in the input word";
    BVec sumOfOnes = bitcount(data);
    HCL_NAMED(sumOfOnes);

    HCL_COMMENT << "Prepare XORed and XNORed data words to select from based on number of high bits";

    Bit invert = (sumOfOnes > 4u) | (sumOfOnes == 4u & !data.lsb());

    HCL_COMMENT << "Decode using 1=xor, 0=xnor";
    BVec tmdsReduced = pack(~invert, data);
    for (auto i : utils::Range<size_t>(1, data.size()))
        tmdsReduced[i] ^= tmdsReduced[i - 1] ^ invert;

    HCL_NAMED(tmdsReduced);
    return tmdsReduced;
}

BVec tmdsDecodeReduceTransitions(const BVec& data)
{
    BVec decoded = data(0, data.size() - 1);
    decoded ^= decoded << 1;
    decoded(1, decoded.getWidth() - 1) ^= ~data.msb();

    HCL_NAMED(decoded);
    return decoded;
}

BVec tmdsEncodeBitflip(const Clock& clk, const BVec& data)
{
    HCL_COMMENT << "count the number of uncompensated ones";
    BVec global_counter = 3_b;
    HCL_NAMED(global_counter);

    // TODO: depend with and start value on data width
    BVec word_counter = "b100";
    for (const Bit& b : data)
        word_counter += b;

    Bit invert = word_counter.msb() == global_counter.msb();
    IF ((global_counter == 0) | (word_counter == 0))
        invert = ~data.msb();
    HCL_NAMED(invert);

    BVec result = pack(invert, data.msb(), data(0, -1) ^ invert);
    HCL_NAMED(result);

    // sub or add depending on invert
    global_counter += (word_counter ^ invert) + invert;
    global_counter = clk(global_counter, 0);

    return result;
}

BVec tmdsDecodeBitflip(const BVec& data)
{
    return pack(data[data.size() - 2], data(0, -2) ^ data.msb());
}

TmdsEncoder::TmdsEncoder(Clock& clk) :
    m_Clk{clk}
{
    m_Channel.fill("b1101010100"); // no data symbol
    setName(m_Channel, "channelBlank");
}

void TmdsEncoder::addSync(const Bit& hsync, const Bit& vsync)
{
    GroupScope ent{ GroupScope::GroupType::ENTITY };
    ent.setName("tmdsEncoderSync");

    IF(hsync)
        setSync(true, false);
    IF(vsync)
        setSync(false, true);
    IF(hsync & vsync)
        setSync(true, true);

    setName(m_Channel, "channelSync");
}

void gtry::scl::hdmi::TmdsEncoder::setColor(const ColorRGB& color)
{
    m_Channel[0] = tmdsEncodeSymbol(color.b);
    m_Channel[1] = tmdsEncodeSymbol(color.g);
    m_Channel[2] = tmdsEncodeSymbol(color.r);
    setName(m_Channel, "channelColor");
}

void gtry::scl::hdmi::TmdsEncoder::setSync(bool hsync, bool vsync)
{
    if (hsync && vsync)
        m_Channel[0] = "b1010101011";
    else if (hsync)
        m_Channel[0] = "b0010101011";
    else if (vsync)
        m_Channel[0] = "b0101010100";
    else
        m_Channel[0] = "b1101010100";
}

void gtry::scl::hdmi::TmdsEncoder::setTERC4(BVec ctrl)
{
    std::array<BVec, 16> trec4lookup = {
        "b1010011100",
        "b1001100011",
        "b1011100100",
        "b1011100010",
        "b0101110001",
        "b0100011110",
        "b0110001110",
        "b0100111100",
        "b1011001100",
        "b0100111001",
        "b0110011100",
        "b1011000110",
        "b1010001110",
        "b1001110001",
        "b0101100011",
        "b1011000011"
    };

    HCL_ASSERT(ctrl.getWidth() == 12);
    m_Channel[0] = mux(ctrl(0, 4), trec4lookup); // TODO: improve mux to accept any container as second input
    m_Channel[1] = mux(ctrl(2, 4), trec4lookup); // TODO: subrange as argument for mux
    m_Channel[2] = mux(ctrl(4, 4), trec4lookup);
}

SerialTMDS gtry::scl::hdmi::TmdsEncoder::serialOutput() const
{
    // TODO: use shift register/serdes lib for automatic vendor specific serdes usage

    Clock fastClk = m_Clk.deriveClock(ClockConfig{}.setFrequencyMultiplier(10).setName("TmdsEncoderFastClock"));

    Register<BVec> chan[3];
    chan[0].setClock(fastClk);
    chan[1].setClock(fastClk);
    chan[2].setClock(fastClk);
    
    for (auto& c : chan)
        c = c.delay(1) >> 1;

    Register<BVec> shiftCounter(4_b);
    shiftCounter.setReset("4b0");
    shiftCounter.setClock(fastClk);
    HCL_NAMED(shiftCounter);
    shiftCounter = shiftCounter.delay(1) + 1u;

    IF(shiftCounter == 9)
    {
        shiftCounter = 0;

        for (size_t i = 0; i < m_Channel.size(); ++i)
            chan[i] = m_Channel[i]; // TODO: clock domain crossing lib and warning
    }

    SerialTMDS out;
    // TODO: convinient method to name signals in structs

    for (size_t i = 0; i < m_Channel.size(); ++i)
    {
        out.data[i] = chan[i].lsb();
    }

    return SerialTMDS();
}

SerialTMDS gtry::scl::hdmi::TmdsEncoder::serialOutputInPixelClock(Bit& tick) const
{
    GroupScope ent{ GroupScope::GroupType::ENTITY };
    ent.setName("tmdsEncoderSerializer");

    std::array<BVec, 3> channels = constructFrom(m_Channel);
    BVec counter = 4_b;
    counter += 1;

    tick = '0';
    IF(counter == 10)
    {
        counter = 0;
        channels = m_Channel;
        tick = '1';
    }

    SerialTMDS out;
    out.clock = reg(counter > 4);

    counter = reg(counter, 0);
    channels = reg(channels);

    for (size_t i = 0; i < channels.size(); ++i)
        out.data[i] = channels[i].lsb();

    for (BVec& c : channels)
        c = rotr(c, 1);

    HCL_NAMED(out);
    return out;
}

}
