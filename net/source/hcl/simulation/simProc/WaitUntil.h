#pragma once

#include "../../hlim/NodePort.h"

#include <coroutine>

namespace hcl::core::sim {

class WaitUntil {
    public:
        enum Trigger {
            HIGH,
            LOW,
            RISING,
            FALLING,
            CHANGING
        };

        WaitUntil(hlim::NodePort np, Trigger trigger = HIGH);

        bool await_ready() noexcept { return false; } // always force reevaluation
        void await_suspend(std::coroutine_handle<> handle);
        void await_resume() noexcept { }
    protected:
        hlim::NodePort m_np;
        Trigger m_trigger;
};

}