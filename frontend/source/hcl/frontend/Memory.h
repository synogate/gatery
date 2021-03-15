#pragma once

#include "Bit.h"
#include "BitVector.h"
#include "BitWidth.h"
#include "ConditionalScope.h"
#include "Pack.h"
#include "Clock.h"
#include "SignalMiscOp.h"


#include <hcl/hlim/supportNodes/Node_Memory.h>
#include <hcl/hlim/supportNodes/Node_MemPort.h>
#include <hcl/hlim/NodePtr.h>


#include <functional>

namespace hcl::core::frontend {

    template<typename Data>
    class MemoryPortFactory {
        public:
            MemoryPortFactory(hlim::Node_Memory *memoryNode, const BVec &address, Data defaultValue) : m_memoryNode(memoryNode), m_defaultValue(defaultValue) {
                m_address = address;
                m_wordSize = width(m_defaultValue);
            }

            Data read() const
            {
                auto *readPort = DesignScope::createNode<hlim::Node_MemPort>(m_wordSize);
                readPort->connectMemory(m_memoryNode);
                if (auto* scope = hcl::core::frontend::ConditionalScope::get())
                    readPort->connectEnable(scope->getFullCondition());

                readPort->connectAddress(m_address.getReadPort());

                BVec rawData(SignalReadPort({.node=readPort, .port=(unsigned)hlim::Node_MemPort::Outputs::rdData}));
                Data ret = m_defaultValue;
                hcl::core::frontend::unpack(rawData, ret);
                return ret;
            }

            operator Data() const { return read(); }

            void write(const Data& value) {
                BVec packedValue = pack(value);

                HCL_DESIGNCHECK_HINT(packedValue.size() == m_wordSize, "The width of data assigned to a memory write port must match the previously specified word width of the memory or memory view.");

                auto *writePort = DesignScope::createNode<hlim::Node_MemPort>(m_wordSize);
                writePort->connectMemory(m_memoryNode);
                //writePort->connectEnable(constructEnableBit().getReadPort());
                if (auto* scope = hcl::core::frontend::ConditionalScope::get()) {
                    writePort->connectEnable(scope->getFullCondition());
                    writePort->connectWrEnable(scope->getFullCondition());
                }
                writePort->connectAddress(m_address.getReadPort());
                writePort->connectWrData(packedValue.getReadPort());
                writePort->setClock(ClockScope::getClk().getClk());
            }

            MemoryPortFactory<Data>& operator = (const Data& value) { write(value); return *this; }
        protected:
            hlim::NodePtr<hlim::Node_Memory> m_memoryNode;
            Data m_defaultValue;
            BVec m_address;
            std::size_t m_wordSize;

            Bit constructEnableBit() const {
                Bit enable;
                if (auto* scope = hcl::core::frontend::ConditionalScope::get())
                    enable = hcl::core::frontend::SignalReadPort{ scope->getFullCondition() };
                else
                    enable = '1';
                return enable;
            }
    };

    template<typename Data>
    Data reg(MemoryPortFactory<Data> readPort) { return reg((Data)readPort); }

    template<typename Data>
    void sim_tap(MemoryPortFactory<Data> readPort) { sim_tap((Data)readPort); }


    using MemType = hlim::Node_Memory::MemType;

    template<typename Data>
    class Memory {
        public:
            Memory(std::size_t count, Data def = Data{}) {
                setup(count, std::move(def));
            }

            Memory() { }

            void setup(std::size_t count, Data def = Data{}) {
                HCL_DESIGNCHECK(m_memoryNode == nullptr);
                m_defaultValue = std::move(def);
                m_wordWidth = width(m_defaultValue);
                m_memoryNode = DesignScope::createNode<hlim::Node_Memory>();
                sim::DefaultBitVectorState state;
                state.resize(count * m_wordWidth);
                state.clearRange(sim::DefaultConfig::DEFINED, 0, count * m_wordWidth);
                m_memoryNode->setPowerOnState(std::move(state));
            }


            void setType(MemType type) { m_memoryNode->setType(type); }
            void noConflicts() { m_memoryNode->setNoConflicts(); }
            bool valid() { return m_memoryNode != nullptr; }

            void setPowerOnState(sim::DefaultBitVectorState powerOnState) { m_memoryNode->setPowerOnState(std::move(powerOnState)); }
            void setPowerOnStateZero() {
                auto &state = m_memoryNode->getPowerOnState();
                state.clearRange(sim::DefaultConfig::VALUE, 0, state.size());
                state.setRange(sim::DefaultConfig::DEFINED, 0, state.size());
            }
            //void setReset(std::size_t address, Data constWord);

            void addResetLogic(std::function<BVec(BVec)> address2data);

            std::size_t size() const { return m_memoryNode->getSize(); }
            BitWidth wordSize() const { return BitWidth{m_wordWidth}; }
            BitWidth addressWidth() const { return BitWidth{ utils::Log2C(numWords()) }; }
            std::size_t numWords() const { return size() / m_wordWidth; }

            MemoryPortFactory<Data> operator [] (const BVec& address)             { HCL_DESIGNCHECK(m_memoryNode != nullptr); return MemoryPortFactory<Data>(m_memoryNode, address, m_defaultValue); }
            const MemoryPortFactory<Data> operator [] (const BVec& address) const { HCL_DESIGNCHECK(m_memoryNode != nullptr); return MemoryPortFactory<Data>(m_memoryNode, address, m_defaultValue); }

            template<typename DataNew>
            Memory<DataNew> view(DataNew def = DataNew{}) { return Memory<DataNew>(m_memoryNode, def); }

        protected:
            hlim::NodePtr<hlim::Node_Memory> m_memoryNode;
            Data m_defaultValue;
            std::size_t m_wordWidth = 0;

            Memory(hlim::Node_Memory *memoryNode, Data def = Data{}) : m_memoryNode(memoryNode) {
            }

    };

    template<typename DataOld, typename DataNew>
    Memory<DataNew> view(Memory<DataOld> old, DataNew def = DataNew{}) { return old.view(def); }

}
