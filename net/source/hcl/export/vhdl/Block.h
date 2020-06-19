#pragma once

#include "BasicBlock.h"

#include <ostream>


namespace mhdl::core::vhdl {

class Entity;
    
/**
 * @todo write docs
 */
class Block : public BasicBlock
{
    public:
        Block(Entity *parent, const std::string &desiredName);
        virtual ~Block();

        void buildFrom(hlim::NodeGroup *nodeGroup);
        
        void writeVHDL(std::ostream &stream);
    protected:
        
};


}
