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
#pragma once

#include <gatery/frontend.h>

#include "../Stream.h"

namespace gtry::scl {

    enum class PortConflict
    {
        inOrder,
        dontCare
    };

    namespace internal
    {
        using namespace gtry;

        struct MemoryPort
        {
            MemoryPort(const BVec& addr) : address(addr) {}

            BVec address;

            std::optional<Bit> write;
            std::optional<BVec> writeData;
        };

        struct Memory
        {
            std::vector<BVec> data;
            std::vector<BVec> readData;
            std::map<gtry::SignalReadPort, MemoryPort> ports;

            PortConflict samePortRead = PortConflict::inOrder;
            PortConflict differentPortRead = PortConflict::inOrder;
            PortConflict differentPortWrite = PortConflict::inOrder;
        };
    }

    template<typename Data>
    class MemoryReadPort
    {
    public:
        MemoryReadPort(const MemoryReadPort&) = default;
        MemoryReadPort(std::shared_ptr<internal::Memory> mem, internal::MemoryPort& port, const Data& defaultValue) :
            m_memory(mem),
            m_port(port),
            m_defaultValue(defaultValue)
        {}

        MemoryReadPort& byteEnable(const BVec&);

        Data read() const
        {
            BVec readData = mux(m_port.address, m_memory->readData);

            for (auto& it : m_memory->ports)
            {
                if (it.second.write)
                {
                    if (&it.second == &m_port && m_memory->samePortRead != PortConflict::dontCare)
                    {
                        IF(*it.second.write)
                            readData = *it.second.writeData;
                    }

                    if (&it.second != &m_port && m_memory->differentPortRead != PortConflict::dontCare)
                    {
                        IF(*it.second.write & it.second.address == m_port.address)
                            readData = *it.second.writeData;
                    }
                }
            }

            Data ret = m_defaultValue;
            gtry::unpack(readData, ret);
            return ret;
        }

        operator Data() const { return read(); }

    protected:
        std::shared_ptr<internal::Memory> m_memory;
        internal::MemoryPort& m_port;
        Data m_defaultValue;

    };

    template<typename Data>
    class MemoryPort : public MemoryReadPort<Data>
    {
    public:
        MemoryPort(const MemoryPort&) = default;
        using MemoryReadPort<Data>::MemoryReadPort;

        MemoryPort& byteEnable(const BVec&);

        MemoryPort& write(const Data& value)
        {
            auto* scope = gtry::ConditionalScope::get();
            if (!scope)
                this->m_port.write = '1';
            else
                this->m_port.write = gtry::SignalReadPort{ scope->getFullCondition() };

            this->m_port.writeData = pack(value);
            sim_debug() << "write " << *this->m_port.write << ", data " << *this->m_port.writeData << ", address " << this->m_port.address;

            for (size_t i = 0; i < this->m_memory->data.size(); ++i)
            {
                IF(this->m_port.address == i)
                    this->m_memory->data[i] = *this->m_port.writeData;
            }
            return *this;
        }

        MemoryPort& operator = (const Data& value) { return write(value); }

    };


    template<typename Data = BVec>
    class Rom
    {
    public:
        template<typename DataInit>
        Rom(size_t size, DataInit def = Data{}) :
            m_defaultValue(def)
        {
            m_memory = std::make_shared<internal::Memory>();
            m_memory->data.resize(size);
            m_memory->readData.resize(size);

            for (size_t i = 0; i < m_memory->data.size(); ++i)
            {
                m_memory->data[i] = def;
                m_memory->data[i] = reg(m_memory->data[i]);
                m_memory->readData[i] = m_memory->data[i];
            }
        }

        size_t size() const { return m_memory->data.size(); }

        MemoryReadPort<Data> operator [] (const BVec& address)
        {
            auto readPort = address.getReadPort();
            auto [it, found] = m_memory->ports.try_emplace(readPort, address);
            return { m_memory, it->second, m_defaultValue };
        }

    protected:
        std::shared_ptr<internal::Memory> m_memory;
        Data m_defaultValue;
    };

    template<typename Data = BVec>
    class Ram : public Rom<Data>
    {
    public:
        using Rom<Data>::Rom;
        
        // - merge same bitvector node ports
        // - allow different address width for Data = BVec
        MemoryPort<Data> operator [] (const BVec& address)
        {
            auto readPort = address.getReadPort();
            auto [it, found] = this->m_memory->ports.try_emplace(readPort, address);
            return { this->m_memory, it->second, this->m_defaultValue };
        }
    };



    // temp interface
    struct WritePort
    {
        WritePort(size_t addrWidth, size_t dataWidth) :
            address(gtry::BitWidth{ addrWidth }),
            writeData(gtry::BitWidth{ dataWidth })
        {}

        gtry::BVec address;
        gtry::BVec writeData;
    };

    // TODO remove simpleDualPortRam API
    Stream<BVec> simpleDualPortRam(Stream<WritePort>& write, Stream<BVec> readAddress, std::string_view name);

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::WritePort, address, writeData);
