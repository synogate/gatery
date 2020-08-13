#pragma once

#include "Signal.h"
#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/utils/Exceptions.h>

#include <vector>

namespace hcl::core::frontend {
    
    class BVec;
    
    class Bit : public ElementarySignal
    {
        public:
            using isBitSignal = void;
        
            Bit();
            Bit(const Bit& rhs);

            Bit(const SignalReadPort& port);
            Bit(hlim::Node_Signal* node, size_t offset); // alias Bit

            Bit(char);
            Bit(bool);
        
            Bit& operator=(const Bit& rhs) { assign(rhs.getReadPort()); return *this; }
            Bit& operator=(char rhs) { assign(rhs); return *this; }
            Bit& operator=(bool rhs) { assign(rhs); return *this; }

            const Bit operator*() const;

            size_t getWidth() const final;
            hlim::ConnectionType getConnType() const final;
            SignalReadPort getReadPort() const final;
            std::string_view getName() const final;
            void setName(std::string name) override;

        protected:
            void createNode();
            void assign(bool);
            void assign(char);
            virtual void assign(SignalReadPort in);

            bool valid() const final; // hide method since Bit is always valid

        private:
            hlim::Node_Signal* m_node = nullptr;
            size_t m_offset = 0;

    };


}
