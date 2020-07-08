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

core::frontend::BitVector tmdsReduceTransitions(core::frontend::UnsignedInteger data)
{
    return core::frontend::BitVector();
}

}
