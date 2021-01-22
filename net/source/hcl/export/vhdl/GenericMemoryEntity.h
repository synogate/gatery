#pragma once

#include "Entity.h"

#include "../../hlim/MemoryDetector.h"

namespace hcl::core::vhdl {

    
/**
 * @todo write docs
 */
class GenericMemoryEntity : public Entity
{
    public:
        GenericMemoryEntity(AST &ast, const std::string &desiredName, BasicBlock *parent);
        
        void buildFrom(hlim::MemoryGroup *memGrp);
    protected:        
        hlim::MemoryGroup *m_memGrp;

        virtual void writeLocalSignalsVHDL(std::ostream &stream) override;
        virtual void writeStatementsVHDL(std::ostream &stream, unsigned indent);
};

}
