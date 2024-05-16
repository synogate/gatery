# Gatery {#gtry_page}

[![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://github.com/codespaces/new?hide_repo_select=true&ref=github_codespace_test&repo=368247767)
[![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)](https://www.gnu.org/licenses/lgpl-3.0)
![Activity](https://img.shields.io/github/commit-activity/y/synogate/gatery)

![Gatery logo](doc/gatery_logo_500.svg)

This is the repository of Gatery, a library for circuit design.
Visit [the Gatery website](https://synogate.com/gatery.html) for details, where a [tutorial](https://www.synogate.com/gatery/tutorial/part1.html) is also available.
For a quick glance, explore the [project template in a github codespace](https://github.com/codespaces/new?hide_repo_select=true&ref=github_codespace_test&repo=368247767).

[TOC]

## Prerequisites

- C++ 20 compiler
- Boost
- make or MS VisualStudio
- premake5

On Fedora: 

```bash
# install gcc, boost, git (for cloning)
sudo dnf install g++ boost-devel git make gmp-devel yaml-cpp-devel
# verify gcc10 or later
gcc --version

# fetch premake5 and make globally available
curl -L https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-linux.tar.gz > /tmp/premake-5.0.0-beta2-linux.tar.gz
tar -zxf  /tmp/premake-5.0.0-beta2-linux.tar.gz -C /tmp/
sudo mv /tmp/premake5 /usr/local/bin/

# fetch template project
git clone --recursive https://github.com/synogate/gatery_template.git ~/Documents/gatery/hello_world/

# Generate makefiles
cd ~/Documents/gatery/hello_world/
premake5 gmake2
```

On Ubuntu:

```bash
# Ubuntu is slightly more involved as gcc10 is a separate package. Also boost needs to be build from scratch since the repository version is not compatible with c++20.

# install gcc, boost, git (for cloning)
sudo apt install build-essential g++-10 libboost-all-dev git libgmp-dev libyaml-cpp-dev
# Select gcc10 as default gcc
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 10
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 10
sudo update-alternatives --config gcc
sudo update-alternatives --config g++
# verify gcc10 or later
gcc --version

# fetch premake5 and make globally available
curl -L https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-linux.tar.gz > /tmp/premake-5.0.0-beta2-linux.tar.gz
tar -zxf  /tmp/premake-5.0.0-beta2-linux.tar.gz -C /tmp/
sudo mv /tmp/premake5 /usr/local/bin/

# Fetch, build, and install boost
curl -L https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_1_76_0.tar.gz > /tmp/boost_1_76_0.tar.gz
mkdir -p ~/Documents/software/
tar -zxf  /tmp/boost_1_76_0.tar.gz -C ~/Documents/software/
cd ~/Documents/software/boost_1_76_0
./bootstrap.sh
./b2
sudo ./b2 install

# fetch template project
git clone --recursive https://github.com/synogate/gatery_template.git ~/Documents/gatery/hello_world/

# Generate makefiles
cd ~/Documents/gatery/hello_world/
premake5 gmake2
```

On Windows:

```bash
# Go to visualstudio.microsoft.com and download/install Visual Studio with the packages for C++ development and Git for Windows. Also install from individual components "MSVC v142 - VS 2019 C++ x84/x86 build tools".

#Commands work only with Windows PowerShell!

# Assume Visual Studio and Git for Windows installed
# Get boost via vcpkg, so fetch and install vcpkg (a package manager)
git clone https://github.com/microsoft/vcpkg "$Env:USERPROFILE/Documents/vcpkg_target_dir"
cd "$Env:USERPROFILE/Documents/vcpkg_target_dir"
.\bootstrap-vcpkg.bat
# Fetch and build boost (this may take a while)
.\vcpkg.exe install boost:x64-windows
.\vcpkg.exe integrate install

# Fetch and build premake
git clone https://github.com/premake/premake-core "$Env:USERPROFILE/Documents/premake_target_dir"
cd "$Env:USERPROFILE/Documents/premake_target_dir"
.\Bootstrap.bat

# fetch template project
git clone --recursive https://github.com/synogate/gatery_template.git "$Env:USERPROFILE/Documents/gatery/hello_world/"
cd "$Env:USERPROFILE/Documents/gatery/hello_world/"
# Generate visual studio solutions with premake
&"$Env:USERPROFILE/Documents/premake_target_dir/bin/release/premake5.exe" vs2022
```

## Getting started

````cpp
#include <gatery/frontend.h>
#include <gatery/utils.h>
#include <gatery/export/vhdl/VHDLExport.h>

#include <gatery/scl/arch/intel/IntelDevice.h>
#include <gatery/scl/synthesisTools/IntelQuartus.h>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/simulation/ReferenceSimulator.h>

using namespace gtry;
using namespace gtry::vhdl;
using namespace gtry::scl;
using namespace gtry::utils; 

int main()
{
	DesignScope design;

	// Optional: Set target technology
	{
		auto device = std::make_unique<IntelDevice>();
		device->setupCyclone10();
		design.setTargetTechnology(std::move(device));
	}

	// Build circuit
	Clock clock{{.absoluteFrequency = 1'000}}; // 1KHz
	ClockScope clockScope{ clock };

	hlim::ClockRational blinkFrequency{1, 1}; // 1Hz
	size_t counterMax = hlim::floor(clock.absoluteFrequency() / blinkFrequency);
	UInt counter = BitWidth(utils::Log2C(counterMax+1));
	auto enable = pinIn().setName("button");
	
	IF (enable)
		counter += 1;

	counter = reg(counter, 0);
	HCL_NAMED(counter);

	pinOut(counter.msb()).setName("led");

	design.postprocess();

	// Optional: Setup simulation
	sim::ReferenceSimulator simulator;
	simulator.compileProgram(design.getCircuit());

	simulator.addSimulationProcess([=, &clock]()->SimProcess{
		simu(enable) = '0';
		for ([[maybe_unused]]auto i : Range(300))
			co_await AfterClk(clock);

		simu(enable) = '1';
	});


	// Optional: Record simulation waveforms as VCD file
	sim::VCDSink vcd(design.getCircuit(), simulator, "waveform.vcd");
	vcd.addAllPins();
	vcd.addAllSignals();


	// Optional: VHDL export
	VHDLExport vhdl("vhdl/");
	vhdl.addTestbenchRecorder(simulator, "testbench", false);
	vhdl.targetSynthesisTool(new IntelQuartus());
	vhdl.writeProjectFile("import_IPCore.tcl");
	vhdl.writeStandAloneProjectFile("IPCore.qsf");
	vhdl.writeConstraintsFile("constraints.sdc");
	vhdl.writeClocksFile("clocks.sdc");
	vhdl(design.getCircuit());

	// Run simulation
	simulator.powerOn();
	simulator.advance(hlim::ClockRational(5000,1'000));

	return 0;
}
````

## Further Documentation

- @subpage gtry_frontend_page
- @subpage gtry_synthesisTools_page
- @subpage gtry_scl_page


## License

This file is part of Gatery, a library for circuit design.
Copyright (C) 2021-2022 Michael Offel, Andreas Ley

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


@defgroup gtry Gatery
See @ref gtry_page