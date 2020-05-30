#include "VpiModule.h"
#include "ClockGenerator.h"

#include <fstream>
#include <optional>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/process/environment.hpp>
#include <vpi_user.h>

namespace ipc = boost::interprocess;

namespace vpi_host
{
    class VpiSimulationHost
    {
    public:
        
        bool onLoadLibrary();
        void onEndOfCompile();

    protected:
        void executeCommand();

    private:
        std::optional<ipc::message_queue> m_CmdQueue;
        std::vector<uint8_t> m_CmdBuffer;
    };

    bool VpiSimulationHost::onLoadLibrary()
    {
        std::string commName = boost::this_process::environment().get("MHDL_VPI_CMDQUEUE");
        if (commName.empty())
            return false;

        m_CmdQueue.emplace(ipc::open_only, commName.c_str());        
        return true;
    }

    void VpiSimulationHost::onEndOfCompile()
    {
        executeCommand();
    }

    void VpiSimulationHost::executeCommand()
    {
        m_CmdBuffer.resize(m_CmdQueue->get_max_msg_size());

        size_t len, prio;
        m_CmdQueue->receive(m_CmdBuffer.data(), m_CmdBuffer.size(), len, prio);
        m_CmdBuffer.resize(len);

        if (!len)
            throw std::runtime_error{ "unexpected empty command message" };
        
        switch (m_CmdBuffer[0])
        {
        case 'e':
            return; // exit simulation
        default:
            throw std::runtime_error{"unknown command message code"};
        }
    }
}

static vpi_host::VpiSimulationHost g_Host;


static PLI_INT32 on_end_of_compile(t_cb_data*)
{
    g_Host.onEndOfCompile();

    vpi_host::VpiModule root;
    auto info = root.simInfo();

    std::ofstream f{ "test.archive", std::ios_base::binary };
    boost::archive::binary_oarchive oa{ f };
    oa << info;

    return 0;
}


extern "C" void my_handle_register()
{
    if (g_Host.onLoadLibrary())
    {
        s_cb_data cb{ cbEndOfCompile, &on_end_of_compile };
        vpi_register_cb(&cb);
    }
}

void (*vlog_startup_routines[]) () =
{
    my_handle_register,
    0
};
