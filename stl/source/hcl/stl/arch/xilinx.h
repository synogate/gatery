#pragma once

#include <stddef.h>

namespace hcl::core::hlim {
    class Circuit;
}

namespace hcl::stl {

struct XilinxSettings {
    enum Series {
        xil_7series
    };

    size_t optimizationLevel = 3;
    Series series = xil_7series;
};


void adaptToArchitecture(core::hlim::Circuit &circuit, const XilinxSettings &settings = {});

}