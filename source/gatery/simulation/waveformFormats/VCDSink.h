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

#include "../WaveformRecorder.h"

#include <fstream>
#include <string>
#include <map>

namespace gtry::sim {

class VCDSink : public WaveformRecorder
{
    public:
        VCDSink(hlim::Circuit &circuit, Simulator &simulator, const char *filename, const char *logFilename = nullptr);

        virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
        virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
        virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;
        virtual void onClock(const hlim::Clock *clock, bool risingEdge) override;
    protected:
        std::fstream m_vcdFile;
        std::fstream m_logFile;
        bool m_doWriteLogFile;

        std::vector<std::string> m_id2sigCode;
        std::map<hlim::Clock*, std::string> m_clock2code;
        std::vector<hlim::Clock*> m_allClocks;

        virtual void initialize() override;
        virtual void signalChanged(size_t id) override;
        virtual void advanceTick(const hlim::ClockRational &simulationTime) override;

        void stateToFile(size_t offset, size_t size);
};

}
