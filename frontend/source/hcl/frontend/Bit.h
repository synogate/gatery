#pragma once

#include "Signal.h"
#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/utils/Exceptions.h>

#include <vector>
#include <optional>

namespace hcl::core::frontend {
    
    class BVec;
    
    class Bit : public ElementarySignal
    {
        public:
            using isBitSignal = void;
        
            Bit();
            Bit(const Bit& rhs);
            Bit(Bit&& rhs);
            ~Bit();

            Bit(const SignalReadPort& port);
            Bit(hlim::Node_Signal* node, size_t offset); // alias Bit

            template<typename T, typename = std::enable_if_t<std::is_same_v<T, char> || std::is_same_v<T, bool>>>
            Bit(T v) {
                createNode();
                assign(v);
            }
        
            Bit& operator=(const Bit& rhs) { assign(rhs.getReadPort()); return *this; }
            Bit& operator=(Bit&& rhs);

            template<typename T, typename = std::enable_if_t<std::is_same_v<T, char> || std::is_same_v<T, bool>>>
            Bit& operator=(T rhs) { assign(rhs); return *this; }

            BitWidth getWidth() const final;
            hlim::ConnectionType getConnType() const final;
            SignalReadPort getReadPort() const final;
            std::string_view getName() const final;
            void setName(std::string name) override;
            void addToSignalGroup(hlim::SignalGroup *signalGroup);

            void setResetValue(bool v);
            void setResetValue(char v);
            std::optional<bool> getResetValue() const { return m_resetValue; }

        protected:
            void createNode();
            void assign(bool);
            void assign(char);
            virtual void assign(SignalReadPort in);

            bool valid() const final; // hide method since Bit is always valid
            SignalReadPort getRawDriver() const;

        private:
            hlim::Node_Signal* m_node = nullptr;
            size_t m_offset = 0;
            std::optional<bool> m_resetValue;
    };

}
