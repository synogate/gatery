#include "ReferenceSimulator.h"

#include "../utils/Range.h"
#include "../hlim/Circuit.h"
#include "../hlim/coreNodes/Node_Constant.h"
#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/coreNodes/Node_Register.h"
#include "../hlim/coreNodes/Node_Arithmetic.h"
#include "../hlim/coreNodes/Node_Logic.h"
#include "../hlim/coreNodes/Node_Multiplexer.h"
#include "../hlim/coreNodes/Node_PriorityConditional.h"
#include "../hlim/coreNodes/Node_Rewire.h"
#include "../hlim/NodeVisitor.h"

#include <iostream>

#include <immintrin.h>

namespace mhdl::core::sim {
    
    

    

unsigned nextPow2(unsigned v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}
    
class BitAllocator {
    public:
        enum {
            BUCKET_1,
            BUCKET_2,
            BUCKET_4,
            BUCKET_8,
            BUCKET_16,
            BUCKET_32,
            NUM_BUCKETS
        };

        size_t allocate(unsigned size) {
            if (size <= 32) {
                size = nextPow2(size);
                
                unsigned bucket;
                switch (size) {
                    case 1: bucket = BUCKET_1; break;
                    case 2: bucket = BUCKET_2; break;
                    case 4: bucket = BUCKET_4; break;
                    case 8: bucket = BUCKET_8; break;
                    case 16: bucket = BUCKET_16; break;
                    case 32: bucket = BUCKET_32; break;
                }
                
                if (m_buckets[bucket].remaining == 0) {
                    m_buckets[bucket].offset = m_totalSize;
                    m_totalSize += 64;
                    m_buckets[bucket].remaining = 64 / size;
                }
                size_t offset = m_buckets[bucket].offset;
                m_buckets[bucket].offset += size;
                m_buckets[bucket].remaining--;
                return offset;
            } else {
                size = (size + 63ull) & ~63ull;
                size_t offset = m_totalSize;
                m_totalSize += size;
                return offset;
            }
        }
        
        void flushBuckets() {
            for (auto i : utils::Range<unsigned>(NUM_BUCKETS)) {
                m_buckets[i].remaining = 0;
            }
        }
        
        size_t getTotalSize() const { return m_totalSize; }
    protected:
        struct Bucket {
            size_t offset = 0;
            unsigned remaining = 0;
        };
        std::array<Bucket, NUM_BUCKETS> m_buckets;
        size_t m_totalSize = 0;
};
    
    
    
    
DataState DataState::getRange(size_t offset, size_t size)
{
    DataState result;
    
    ///@todo optimze, make use of alignments
    result.resize(size);
    for (auto i : utils::Range(size)) {
        result.setBit(i, bit(i+offset));
        result.setDefined(i, defined(i+offset));
    }
    
    return result;
}

bool DataState::anyUndefined(size_t offset, size_t size)
{
    while (size > 0) {
        if (size <= 64) {
            std::uint64_t block = m_defined[offset/64];
            size_t blockOffset = offset % 64;
            block >>= blockOffset;
            std::uint64_t mask = ~0ull >> (64-size);
            
            return ~block & mask;
        } else {
            std::uint64_t block = m_defined[offset/64];
            
            if (~block) 
                return true;
            
            size -= 64;
            offset += 64;
        }
    }
    return false;
}

    
void DataState::resize(size_t size) 
{ 
    m_size = size;
    
    // padd to multiple of 64 bits
    size = (size + 63ull) / 64;
    m_values.resize(size); 
    m_defined.resize(size); 
}

void DataState::clear() 
{ 
    m_values.clear(); 
    m_defined.clear(); 
    m_size = 0;
}

    
void ExecutionBlock::execute(DataState &state) const
{
    for (const auto &inst : m_instructions) 
        inst(state);
}
    
void ExecutionBlock::addInstruction(const std::function<void(DataState &state)> &inst)
{
    m_instructions.push_back(inst);
}

Register::Register(unsigned size, unsigned srcData, unsigned srcReset, unsigned dst)
{
    m_dataWordSize = size;
    m_srcDataWordOffset = srcData;
    m_dstDataWordOffset = dst;
    m_resetDataWordOffset = srcReset;
}



class Node2Instruction : public hlim::ConstNodeVisitor
{
    public:
        Node2Instruction(ExecutionBlock &execBlock, StateMapping &stateMapping) : m_execBlock(execBlock), m_stateMapping(stateMapping) { }
        
        size_t getInputOffsets(const hlim::BaseNode *node, size_t port) {
            auto driver = node->getNonSignalDriver(port);
            return m_stateMapping.outputToOffset[driver];
        }

        size_t getOutputOffsets(const hlim::BaseNode *node, size_t port) {
            hlim::NodePort driver;
            driver.node = (hlim::BaseNode *) node;
            driver.port = port;
            return m_stateMapping.outputToOffset[driver];
        }
        
        virtual void operator()(const hlim::Node_Arithmetic &node) override {
            auto inA = getInputOffsets(&node, 0);
            auto inB = getInputOffsets(&node, 1);
            auto out = getOutputOffsets(&node, 0);
            
            ///@todo different input widths!
            size_t width = node.getOutputConnectionType(0).width;
            
            MHDL_ASSERT_HINT(width <= 64, "Large integers not implemented yet!");
            
            switch (node.getOp()) {
                case hlim::Node_Arithmetic::ADD:
                    if ((inA != ~0ull) && (inB != ~0ull))
                        m_execBlock.addInstruction([inA, inB, out, width](DataState &state){
                            ///@todo opextend
                            
                            std::uint64_t mask = (~0ull >> (64 - width)) << (out % 64);
                            if (state.anyUndefined(inA, width) || state.anyUndefined(inB, width)) {
                                state.getDefinedData()[out / 64] &= ~mask;
                            } else {
                                std::uint64_t opA = state.getValueData()[inA / 64] >> (inA % 64);
                                std::uint64_t opB = state.getValueData()[inB / 64] >> (inB % 64);
                                /*
                                std::uint64_t opA = _bextr_u64(state.getValueData()[inA / 64], inA % 64, width);
                                std::uint64_t opB = _bextr_u64(state.getValueData()[inB / 64], inB % 64, width);
                                */
                                std::uint64_t res = opA + opB;
                                state.getValueData()[out / 64] = (state.getValueData()[out / 64] & ~mask) | ((res << (out % 64)) & mask);
                                state.getDefinedData()[out / 64] |= mask;
                            }
                            
                        });
                break;
                default:
                    MHDL_ASSERT_HINT(false, "Not implemented!");
            }
        }
        virtual void operator()(const hlim::Node_Compare &node) override {
            MHDL_ASSERT_HINT(false, "Not implemented!");
        }
        virtual void operator()(const hlim::Node_Constant &node) override {
            MHDL_ASSERT(false);
        }
        virtual void operator()(const hlim::Node_Logic &node) override {
            ///@todo different input widths!
            size_t width = node.getOutputConnectionType(0).width;
            
            MHDL_ASSERT_HINT(width <= 64, "Large integers not implemented yet!");
            
            switch (node.getOp()) {
                case hlim::Node_Logic::NOT: {
                    auto inA = getInputOffsets(&node, 0);
                    auto out = getOutputOffsets(&node, 0);
                    if (inA != ~0ull)
                        m_execBlock.addInstruction([inA, out, width](DataState &state){
                            ///@todo opextend
                            
                            std::uint64_t mask = (~0ull >> (64 - width)) << (out % 64);
                            if (state.anyUndefined(inA, width)) {
                                state.getDefinedData()[out / 64] &= ~mask;
                            } else {
                                std::uint64_t opA = state.getValueData()[inA / 64] >> (inA % 64);
                                std::uint64_t res = ~opA;
                                state.getValueData()[out / 64] = (state.getValueData()[out / 64] & ~mask) | ((res << (out % 64)) & mask);
                                state.getDefinedData()[out / 64] |= mask;
                            }
                            
                        });
                } break;
                case hlim::Node_Logic::AND: {
                    auto inA = getInputOffsets(&node, 0);
                    auto inB = getInputOffsets(&node, 1);
                    auto out = getOutputOffsets(&node, 0);
                    
                    if ((inA != ~0ull) && (inB != ~0ull))
                        m_execBlock.addInstruction([inA, inB, out, width](DataState &state){
                            ///@todo opextend
                            
                            std::uint64_t mask = (~0ull >> (64 - width)) << (out % 64);
                            if (state.anyUndefined(inA, width) || state.anyUndefined(inB, width)) {
                                state.getDefinedData()[out / 64] &= ~mask;
                            } else {
                                std::uint64_t opA = state.getValueData()[inA / 64] >> (inA % 64);
                                std::uint64_t opB = state.getValueData()[inB / 64] >> (inB % 64);
                                std::uint64_t res = opA & opB;
                                state.getValueData()[out / 64] = (state.getValueData()[out / 64] & ~mask) | ((res << (out % 64)) & mask);
                                state.getDefinedData()[out / 64] |= mask;
                            }
                            
                        });
                } break;
                case hlim::Node_Logic::OR: {
                    auto inA = getInputOffsets(&node, 0);
                    auto inB = getInputOffsets(&node, 1);
                    auto out = getOutputOffsets(&node, 0);
                    
                    if ((inA != ~0ull) && (inB != ~0ull))
                        m_execBlock.addInstruction([inA, inB, out, width](DataState &state){
                            ///@todo opextend
                            
                            std::uint64_t mask = (~0ull >> (64 - width)) << (out % 64);
                            if (state.anyUndefined(inA, width) || state.anyUndefined(inB, width)) {
                                state.getDefinedData()[out / 64] &= ~mask;
                            } else {
                                std::uint64_t opA = state.getValueData()[inA / 64] >> (inA % 64);
                                std::uint64_t opB = state.getValueData()[inB / 64] >> (inB % 64);
                                std::uint64_t res = opA | opB;
                                state.getValueData()[out / 64] = (state.getValueData()[out / 64] & ~mask) | ((res << (out % 64)) & mask);
                                state.getDefinedData()[out / 64] |= mask;
                            }
                            
                        });
                } break;
                default:
                    MHDL_ASSERT_HINT(false, "Not implemented!");
            }
        }
        virtual void operator()(const hlim::Node_Multiplexer &node) override {
            //MHDL_ASSERT_HINT(false, "Not implemented!");
        }
        virtual void operator()(const hlim::Node_PriorityConditional &node) override {
            //MHDL_ASSERT_HINT(false, "Not implemented!");
        }
        virtual void operator()(const hlim::Node_Register &node) override {
            MHDL_ASSERT(false);
        }
        virtual void operator()(const hlim::Node_Rewire &node) override {
            //MHDL_ASSERT_HINT(false, "Not implemented!");
        }
        virtual void operator()(const hlim::Node_Signal &node) override {
        }
    protected:
        ExecutionBlock &m_execBlock;
        StateMapping &m_stateMapping;
};

    
void Program::compileProgram(const hlim::Circuit &circuit)
{
    allocateSignals(circuit);
    
    
    // registers
    size_t clkIdx = m_stateMapping.clockToClkIdx["clk"];
    size_t resetIdx = m_stateMapping.resetToSignalIdx["reset"];
    m_clockIdx2clockDomain[clkIdx].push_back(m_clockDomains.size());
    m_clockDomains.push_back({
        .clkIdx = clkIdx,
        .resetIdx = resetIdx,
        .registers={}
    });
    
    
    ///@todo More than one clock domain
    ///@todo Don't stuff everything into one execution block
    ///@todo Somehow split state to be multithreading safe
    
    std::set<hlim::NodePort> outputsReady;
    
    std::set<hlim::BaseNode*> nodesRemaining;
    for (const auto &node : circuit.getNodes()) {
        hlim::Node_Constant *constNode = dynamic_cast<hlim::Node_Constant*>(node.get());
        if (constNode != nullptr) {
            outputsReady.insert({.node = constNode, .port = 0ull});
            continue;
        }
        hlim::Node_Register *regNode = dynamic_cast<hlim::Node_Register*>(node.get());
        if (regNode != nullptr) {
            outputsReady.insert({.node = regNode, .port = 0ull});
            
            m_clockDomains[0].registers.push_back(Register(
                regNode->getOutputConnectionType(0).width,
                m_stateMapping.outputToOffset[regNode->getDriver(hlim::Node_Register::DATA)],
                m_stateMapping.outputToOffset[regNode->getDriver(hlim::Node_Register::RESET_VALUE)],
                m_stateMapping.outputToOffset[{.node=regNode, .port=0ull}]
            ));
            continue;
        }
        nodesRemaining.insert(node.get());
    }
    
    m_executionBlocks.push_back({});
    ExecutionBlock &execBlock = m_executionBlocks.back();
    
    

    
    Node2Instruction node2inst(execBlock, m_stateMapping);
    
    
    while (!nodesRemaining.empty()) {
        hlim::BaseNode *readyNode = nullptr;
        for (auto node : nodesRemaining) {
            bool allInputsReady = true;
            for (auto i : utils::Range(node->getNumInputPorts())) {
                auto driver = node->getNonSignalDriver(i);
                if (driver.node != nullptr && (outputsReady.find(node->getNonSignalDriver(i)) == outputsReady.end())) {
                    allInputsReady = false;
                    break;
                }
            }
            
            if (allInputsReady) {
                readyNode = node;
                break;
            }
        }
        
        if (readyNode == nullptr) {
            std::cout << "nodesRemaining : " << nodesRemaining.size() << std::endl;
            
            for (auto node : nodesRemaining) {
                std::cout << node->getName() << "  " << node->getTypeName() << std::endl;
                for (auto i : utils::Range(node->getNumInputPorts())) {
                    auto driver = node->getNonSignalDriver(i);
                    if (driver.node != nullptr && (outputsReady.find(node->getNonSignalDriver(i)) == outputsReady.end())) {
                        std::cout << "    Input " << i << " not ready." << std::endl;
                        std::cout << "        " << driver.node->getName() << "  " << driver.node->getTypeName() << std::endl;
                    }
                }
            }
        }
        
        MHDL_DESIGNCHECK_HINT(readyNode != nullptr, "Cyclic dependency!");
        
        nodesRemaining.erase(readyNode);     
        readyNode->visit(node2inst);

        for (auto i : utils::Range(readyNode->getNumOutputPorts())) {
            hlim::NodePort driver;
            driver.node = readyNode;
            driver.port = i;
            outputsReady.insert(driver);
        }
    }
    
}

void Program::allocateSignals(const hlim::Circuit &circuit)
{
    m_initialState.clear();
    m_stateMapping.clear();
    
    
    BitAllocator allocator;
    
    // constants
    std::vector<hlim::Node_Constant *> constNodes;
    for (const auto &node : circuit.getNodes()) {
        hlim::Node_Constant *constNode = dynamic_cast<hlim::Node_Constant*>(node.get());
        if (constNode != nullptr) {
            constNodes.push_back(constNode);
            const auto &value = constNode->getValue();
            m_stateMapping.outputToOffset[{.node = constNode, .port = 0ull}] = allocator.allocate(value.bitVec.size());
        }
    }
    
    m_initialState.resize(allocator.getTotalSize());
    for (auto constNode : constNodes) {
        const auto &value = constNode->getValue();
        size_t offset = m_stateMapping.outputToOffset[{.node = constNode, .port = 0ull}];
        for (auto i : utils::Range(value.bitVec.size())) {
            m_initialState.setBit(offset + i, value.bitVec[i]);
            m_initialState.setDefined(offset + i, true);
        }
    }

    
    allocator.flushBuckets();
      
    // clocks
    {
        size_t signal = allocator.allocate(1);
        size_t clk = allocator.allocate(1);
        m_clockDrivers.push_back({
            .srcSignalIdx = signal,
            .dstClockIdx = clk,
        });
        
        m_stateMapping.clockToSignalIdx["clk"] = signal;
        m_stateMapping.clockToClkIdx["clk"] = clk;
    }

    
    // resets
    m_stateMapping.resetToSignalIdx["reset"] = allocator.allocate(1);
    

    // variables
    for (const auto &node : circuit.getNodes()) {
        hlim::Node_Signal *signalNode = dynamic_cast<hlim::Node_Signal*>(node.get());
        if (signalNode != nullptr) {
            auto driver = signalNode->getNonSignalDriver(0);
            
            unsigned width = signalNode->getOutputConnectionType(0).width;
            
            if (driver.node != nullptr) {
                auto it = m_stateMapping.outputToOffset.find(driver);
                if (it == m_stateMapping.outputToOffset.end()) {
                    auto offset = allocator.allocate(width);
                    m_stateMapping.outputToOffset[driver] = offset;
                    m_stateMapping.outputToOffset[{.node = signalNode, .port = 0ull}] = offset;
                } else {
                    // point to same output port
                    m_stateMapping.outputToOffset[{.node = signalNode, .port = 0ull}] = it->second;
                }
            }
        } else {
            for (auto i : utils::Range(node->getNumOutputPorts())) {
                hlim::NodePort driver = {.node = node.get(), .port = i};
                auto it = m_stateMapping.outputToOffset.find(driver);
                if (it == m_stateMapping.outputToOffset.end()) {
                    unsigned width = node->getOutputConnectionType(0).width;
                    m_stateMapping.outputToOffset[driver] = allocator.allocate(width);
                }
            }
        }
    }
    
    
    m_fullStateWidth = allocator.getTotalSize();
}
    
    
void ReferenceSimulator::compileProgram(const hlim::Circuit &circuit)
{
    m_program.compileProgram(circuit);
    reset();
}

void ReferenceSimulator::reset()
{
    m_dataState.resize(m_program.getFullStateWidth());
    memcpy(m_dataState.getValueData(), m_program.getInitialState().getValueData(), m_program.getInitialState().getNumBlocks() * sizeof(std::uint64_t));
    memcpy(m_dataState.getDefinedData(), m_program.getInitialState().getDefinedData(), m_program.getInitialState().getNumBlocks() * sizeof(std::uint64_t));
    
    memset(m_dataState.getValueData() + m_program.getInitialState().getNumBlocks(), 
           0x00, (m_dataState.getNumBlocks() - m_program.getInitialState().getNumBlocks()) * sizeof(std::uint64_t));
    memset(m_dataState.getDefinedData() + m_program.getInitialState().getNumBlocks(), 
           0x00, (m_dataState.getNumBlocks() - m_program.getInitialState().getNumBlocks()) * sizeof(std::uint64_t));
}


void ReferenceSimulator::reevaluate()
{
    ///@todo do properly
    
    m_program.getExecutionBlocks().front().execute(m_dataState);
}

void ReferenceSimulator::advanceAnyTick()
{
}


DefaultBitVectorState ReferenceSimulator::getValueOfOutput(const hlim::NodePort &nodePort)
{
    DefaultBitVectorState value;
    auto it = m_program.getStateMapping().outputToOffset.find(nodePort);
    if (it == m_program.getStateMapping().outputToOffset.end()) {
        value.resize(0);
    } else {
        unsigned width = nodePort.node->getOutputConnectionType(nodePort.port).width;
        //value = m_dataState.extract(it->second, width);
    }
    return value;
}

std::array<bool, DefaultConfig::NUM_PLANES> ReferenceSimulator::getValueOfClock(const hlim::BaseClock *clk)
{
}

std::array<bool, DefaultConfig::NUM_PLANES> ReferenceSimulator::getValueOfReset(const std::string &reset)
{
}


}
