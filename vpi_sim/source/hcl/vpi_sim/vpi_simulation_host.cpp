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
        VpiSimulationHost();
        
        void executeCommand();

    protected:

        template<typename T>
        void sendResponse(T& obj);

    private:
        ipc::message_queue m_CmdQueueP2C;
        ipc::message_queue m_CmdQueueC2P;
        std::vector<uint8_t> m_CmdBuffer;
        VpiModule m_Top;
    };

    VpiSimulationHost::VpiSimulationHost() :
        m_CmdQueueP2C(ipc::open_only, boost::this_process::environment().get("HCL_VPI_CMDQUEUE_P2C").c_str()),
        m_CmdQueueC2P(ipc::open_only, boost::this_process::environment().get("HCL_VPI_CMDQUEUE_C2P").c_str())
    {
    }


    void VpiSimulationHost::executeCommand()
    {
        while (true)
        {
            m_CmdBuffer.resize(m_CmdQueueP2C.get_max_msg_size());

            size_t len, prio;
            m_CmdQueueP2C.receive(m_CmdBuffer.data(), m_CmdBuffer.size(), len, prio);
            m_CmdBuffer.resize(len);

            if (!len)
                throw std::runtime_error{ "unexpected empty command message" };

            switch (m_CmdBuffer[0])
            {
            case 'e':
                return; // exit simulation
            case 'I': // siminfo
                sendResponse(m_Top.simInfo());
                break;
            default:
                throw std::runtime_error{ "unknown command message code" };
            }
        }
    }
    
    template<typename T>
    void VpiSimulationHost::sendResponse(T& obj)
    {
        std::ostringstream ss;
        boost::archive::binary_oarchive a{ ss };
        a << obj;

        std::string buffer = ss.str();
        m_CmdQueueC2P.send(buffer.data(), buffer.size(), 0);
    }
}

static std::unique_ptr<vpi_host::VpiSimulationHost> g_Host;


static PLI_INT32 on_end_of_compile(t_cb_data*)
{
    g_Host = std::make_unique<vpi_host::VpiSimulationHost>();
    g_Host->executeCommand();
    return 0;
}

extern "C" void my_handle_register()
{
    s_cb_data cb{ cbEndOfCompile, &on_end_of_compile };
    vpi_register_cb(&cb);
}

void (*vlog_startup_routines[]) () =
{
    my_handle_register,
    0
};
