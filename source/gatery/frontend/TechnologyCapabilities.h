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

#include "Scope.h"

namespace gtry {

enum class Preference {
    DONTCARE,
    SPEED,
    AREA,
    SPECIFIC_VALUE,
    MIN_VALUE,
    MAX_VALUE,
};

template<typename BaseType>
struct Option {
    Preference choice = Preference::DONTCARE;
    BaseType value;

    /// For DONTCARE, SPEED, and AREA
    void operator=(Preference choice) { this->choice = choice; }

    void optimizeDontCare() { choice = Preference::DONTCARE; }
    void optimizeSpeed() { choice = Preference::SPEED; }
    void optimizeArea() { choice = Preference::AREA; }

    void operator=(BaseType v) { choice = Preference::SPECIFIC_VALUE; value = v; }
    void atLeast(BaseType v) { choice = Preference::MIN_VALUE; value = v; }
    void atMost(BaseType v) { choice = Preference::MAX_VALUE; value = v; }
};

namespace details {
    struct RequestWrapper {    
        template<typename BaseType>
        using wrap = Option<BaseType>;
    };

    struct ChoiceWrapper {    
        template<typename BaseType>
        using wrap = BaseType;
    };
}


class Capabilities {
    public:
        virtual ~Capabilities() = default;
};


class SerdesCapabilities : public Capabilities {
    public:
        enum DataRate {
            SDR,
            DDR,
        };

        template<typename Wrapper>
        struct Settings {
            typename Wrapper::wrap<unsigned> width;
            typename Wrapper::wrap<DataRate> dataRate;
        };

        using Request = Settings<details::RequestWrapper>;
        using Choice = Settings<details::ChoiceWrapper>;

    protected:
};

class IOCapabilities : public Capabilities {
};

class MemoryCapabilities : public Capabilities {
    public:

        template<typename Wrapper>
        struct Settings {
            typename Wrapper::wrap<unsigned> width;
            typename Wrapper::wrap<unsigned> depth;
            typename Wrapper::wrap<bool> addrRegister;
            typename Wrapper::wrap<bool> outputRegister;
            struct Port {
                bool canRead;
                bool canWrite;
                // byte enable
                // aspect ratio

                // same port / mixed port Read during write order
                typename Wrapper::wrap<unsigned> order;
            };
            std::vector<Port> ports;
        };

        using Request = Settings<details::RequestWrapper>;
        using Choice = Settings<details::ChoiceWrapper>;
    protected:
};

class FifoCapabilities : public Capabilities {
    public:
        template<typename Wrapper>
        struct Settings {
            typename Wrapper::wrap<unsigned> readWidth;
            typename Wrapper::wrap<unsigned> readDepth;
            typename Wrapper::wrap<unsigned> writeWidth;

			typename Wrapper::wrap<bool> outputIsFallthrough;
			typename Wrapper::wrap<bool> singleClock;

            typename Wrapper::wrap<bool> useECCEncoder;
            typename Wrapper::wrap<bool> useECCDecoder;

			typename Wrapper::wrap<size_t> latency_write_empty;
			typename Wrapper::wrap<size_t> latency_read_empty;
			typename Wrapper::wrap<size_t> latency_write_full;
			typename Wrapper::wrap<size_t> latency_read_full;
			typename Wrapper::wrap<size_t> latency_write_almostEmpty;
			typename Wrapper::wrap<size_t> latency_read_almostEmpty;
			typename Wrapper::wrap<size_t> latency_write_almostFull;
			typename Wrapper::wrap<size_t> latency_read_almostFull;
        };

        using Request = Settings<details::RequestWrapper>;
        using Choice = Settings<details::ChoiceWrapper>;

        virtual ~FifoCapabilities() = default;

        virtual int makeBid(const Request &request, Choice &choice) const { }
    protected:
};


/*
struct Directives {
//    std::regex areaPrefix;

    enum Optimize {
        OPT_DONTCARE,
        OPT_SPEED,
        OPT_AREA,
    };
    Optimize optimize = OPT_DONTCARE;

    //std::map<std::string, std::string> 
};
*/


class TechnologyCapabilities {
    public:
        virtual std::string getName() = 0;
/*
        template<class Cap>
        Cap &operator[](std::string_view type) { 
            auto it = m_capabilities.find(type);
            HCL_DESIGNCHECK_HINT(it != m_capabilities.end(), "Could not find handler for tech capability " + type);
            return dynamic_cast<Cap&>(*it->second);
        }
        */
    protected:
        std::map<std::string, Capabilities*> m_capabilities;
};


class TechnologyScope : public BaseScope<TechnologyScope> {

    public:
        static TechnologyScope *get() { return m_currentScope; }
    protected:

};


}