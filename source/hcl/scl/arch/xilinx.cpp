/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "xilinx.h"

#include "xilinx/OBUFDS.h"

#include <hcl/hlim/Circuit.h>
#include <hcl/hlim/coreNodes/Node_Pin.h>
#include <hcl/hlim/coreNodes/Node_Rewire.h>

namespace hcl::stl {

void handleDifferentialPin(core::hlim::Circuit &circuit, const XilinxSettings &settings, core::hlim::Node_Pin *pin)
{
    HCL_ASSERT_HINT(pin->isOutputPin(), "Differential IO only implemented for output pins!");

    auto driver = pin->getDriver(0);
    auto width = core::hlim::getOutputWidth(driver);


    auto *mergeRewirePos = circuit.createNode<core::hlim::Node_Rewire>(width);
    mergeRewirePos->moveToGroup(pin->getGroup());
    auto *mergeRewireNeg = circuit.createNode<core::hlim::Node_Rewire>(width);
    mergeRewireNeg->moveToGroup(pin->getGroup());

    for (auto i : utils::Range(width)) {
        auto *extractRewire = circuit.createNode<core::hlim::Node_Rewire>(1);
        extractRewire->moveToGroup(pin->getGroup());
        extractRewire->connectInput(0, driver);
        extractRewire->setExtract(i, 1);
        extractRewire->changeOutputType({.interpretation = core::hlim::ConnectionType::BOOL, .width=1});

        auto *buffer = circuit.createNode<arch::xilinx::OBUFDS>();
        buffer->moveToGroup(pin->getGroup());
        buffer->rewireInput(0, {.node = extractRewire, .port = 0ull});

        mergeRewirePos->connectInput(i, {.node = buffer, .port = 0ull});
        mergeRewireNeg->connectInput(i, {.node = buffer, .port = 1ull});
    }

    mergeRewirePos->setConcat();
    mergeRewireNeg->setConcat();

    mergeRewirePos->changeOutputType(driver.node->getOutputConnectionType(driver.port));
    mergeRewireNeg->changeOutputType(driver.node->getOutputConnectionType(driver.port));

    auto *negPin = circuit.createNode<core::hlim::Node_Pin>();
    negPin->moveToGroup(pin->getGroup());
    negPin->setName(pin->getDifferentialNegName());
    pin->setName(pin->getDifferentialPosName());

    pin->rewireInput(0, {.node = mergeRewirePos, .port = 0ull});
    negPin->rewireInput(0, {.node = mergeRewireNeg, .port = 0ull});
    pin->setNormal();
}

void adaptToArchitecture(core::hlim::Circuit &circuit, const XilinxSettings &settings)
{
    for (auto &node : circuit.getNodes())
        if (auto *pin = dynamic_cast<core::hlim::Node_Pin*>(node.get())) {
            if (pin->isDifferential())
                handleDifferentialPin(circuit, settings, pin);
        }

    circuit.optimize(settings.optimizationLevel);
}


}
