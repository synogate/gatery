#pragma once

#include "BasicBlock.h"

#include <string>
#include <vector>
#include <memory>
#include <ostream>

namespace hcl::core::vhdl {

class Block;

/**
 * @todo write docs
 */
class Entity : public BasicBlock
{
    public:
        Entity(AST &ast, const std::string &desiredName, BasicBlock *parent);
        virtual ~Entity();

        inline const std::string &getName() const { return m_name; }

        void buildFrom(hlim::NodeGroup *nodeGroup);

        virtual void extractSignals() override;
        virtual void allocateNames() override;

        void writeVHDL(std::ostream &stream);

        virtual void writeInstantiationVHDL(std::ostream &stream, unsigned indent, const std::string &instanceName);

        Entity *getParentEntity();

        inline const std::vector<std::unique_ptr<Block>> &getBlocks() const { return m_blocks; }
    protected:
        std::vector<std::unique_ptr<Block>> m_blocks;

        virtual void writeLibrariesVHDL(std::ostream &stream);
        virtual std::vector<std::string> getPortsVHDL();
        virtual void writeLocalSignalsVHDL(std::ostream &stream);
};

}
