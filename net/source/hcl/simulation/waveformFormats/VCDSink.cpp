#include "VCDSink.h"

#include "../../hlim/NodeGroup.h"
#include "../../hlim/Circuit.h"

#include <boost/format.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>
#include <functional>

namespace hcl::core::sim {


VCDSink::VCDSink(hlim::Circuit &circuit, Simulator &simulator, const char *filename, const char *logFilename) : WaveformRecorder(circuit, simulator)
{
    m_vcdFile.open(filename, std::fstream::out);
    if (!m_vcdFile) throw std::runtime_error(std::string("Could not open vcd file for writing!") + filename);
    if (logFilename != nullptr) {
        m_logFile.open(logFilename, std::fstream::out);
        if (!m_logFile) throw std::runtime_error(std::string("Could not open log file for writing: ") + logFilename);
    } else
        m_doWriteLogFile = false;

    for (auto &clk : circuit.getClocks())
        m_allClocks.push_back(clk.get());
}

void VCDSink::onDebugMessage(const hlim::BaseNode *src, std::string msg)
{
}

void VCDSink::onWarning(const hlim::BaseNode *src, std::string msg)
{
}

void VCDSink::onAssert(const hlim::BaseNode *src, std::string msg)
{
}

class VCDIdentifierGenerator {
    public:
        enum {
            IDENT_BEG = 33,
            IDENT_END = 127
        };
        VCDIdentifierGenerator() {
            m_nextIdentifier.reserve(10);
            m_nextIdentifier.resize(1);
            m_nextIdentifier[0] = IDENT_BEG;
        }
        std::string getIdentifer() {
            std::string res = m_nextIdentifier;

            unsigned idx = 0;
            while (true) {
                if (idx >= m_nextIdentifier.size()) {
                    m_nextIdentifier.push_back(IDENT_BEG);
                    break;
                } else {
                    m_nextIdentifier[idx]++;
                    if (m_nextIdentifier[idx] >= IDENT_END) {
                        m_nextIdentifier[idx] = IDENT_BEG;
                        idx++;
                    } else break;
                }
            }

            return res;
        }
    protected:
        std::string m_nextIdentifier;
};

void VCDSink::initialize()
{
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    m_vcdFile
        << "$date\n" << std::put_time(std::localtime(&now), "%Y-%m-%d %X") << "\n$end\n"
        << "$version\nGatery simulation output\n$end\n"
        << "$timescale\n1ps\n$end\n";


    VCDIdentifierGenerator identifierGenerator;
    m_id2sigCode.resize(m_signal2id.size());




    struct Module {
        std::map<const hlim::NodeGroup *, Module> subModules;
        std::vector<std::pair<hlim::NodePort, size_t>> signals;
    };

    Module root;

    for (auto &sigId : m_signal2id) {
        m_id2sigCode[sigId.second] = identifierGenerator.getIdentifer();

        std::vector<const hlim::NodeGroup*> nodeGroupTrace;
        const hlim::NodeGroup *grp = sigId.first.node->getGroup();
        while (grp != nullptr) {
            nodeGroupTrace.push_back(grp);
            grp = grp->getParent();
        }
        Module *m = &root;
        for (auto it = nodeGroupTrace.rbegin(); it != nodeGroupTrace.rend(); ++it)
            m = &m->subModules[*it];
        m->signals.push_back(sigId);
    }

    std::function<void(const Module*)> reccurWriteModules;
    reccurWriteModules = [&](const Module *module){
        for (const auto &p : module->subModules) {
            m_vcdFile
                << "$scope module " << p.first->getName() << " $end\n";
            reccurWriteModules(&p.second);
            m_vcdFile
                << "$upscope $end\n";
        }
        for (const auto &sigId : module->signals) {
            auto width = sigId.first.node->getOutputConnectionType(sigId.first.port).width;
            m_vcdFile
                << "$var wire " << width << " " << m_id2sigCode[sigId.second] << " " << m_signalNames[sigId.second] << " $end\n";
        }
    };

    reccurWriteModules(&root);


    m_vcdFile
        << "$scope module clocks $end\n";

    for (auto &clk : m_allClocks) {
        std::string id = identifierGenerator.getIdentifer();
        m_clock2code[clk] = id;
        m_vcdFile
            << "$var wire 1 " << id << " " << clk->getName() << " $end\n";
    }

    m_vcdFile
        << "$upscope $end\n";


    m_vcdFile
        << "$enddefinitions $end\n"
        << "$dumpvars\n";

}

void VCDSink::signalChanged(size_t id)
{
    const auto &offsetSize = m_id2StateOffsetSize[id];
    if (offsetSize.size == 1) {
        stateToFile(offsetSize.offset, 1);
        m_vcdFile << m_id2sigCode[id] << "\n";
    } else {
        m_vcdFile << "b";
        stateToFile(offsetSize.offset, offsetSize.size);
        m_vcdFile << " " << m_id2sigCode[id] << "\n";
    }
}

void VCDSink::advanceTick(const hlim::ClockRational &simulationTime)
{
    auto ratTickIdx = simulationTime / hlim::ClockRational(1, 1'000'000'000'000ull);
    size_t tickIdx = ratTickIdx.numerator() / ratTickIdx.denominator();
    m_vcdFile << "#"<<tickIdx<<"\n";
}


void VCDSink::stateToFile(size_t offset, size_t size)
{
    for (auto i : utils::Range(size)) {
        auto bitIdx = size-1-i;
        bool def = m_trackedState.get(DefaultConfig::DEFINED, offset + bitIdx);
        bool val = m_trackedState.get(DefaultConfig::VALUE, offset + bitIdx);
        if (!def)
            m_vcdFile << 'X';
        else
            if (val)
                m_vcdFile << '1';
            else
                m_vcdFile << '0';
    }
}

void VCDSink::onClock(const hlim::Clock *clock, bool risingEdge)
{
    if (risingEdge)
        m_vcdFile << '1';
    else
        m_vcdFile << '0';

    m_vcdFile << m_clock2code[(hlim::Clock *)clock] << "\n";
}


}