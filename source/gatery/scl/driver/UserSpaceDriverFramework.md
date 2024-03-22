@page gtry_scl_driver_page User Space Driver Framework

Gatery provides a framework with utilities for user space driver implementations.

> This (new) implementation is still experimental and is being rushed out of the door.
> It is very rough, especially the code for translating symbolic register names to addresses.
> Some interfaces will still change!

## Exporting/Importing Memory Maps

Devices are usually controlled by drivers through memory mapped IO which is mapped or otherwise exists in the address space of the driver.
On the circuit side, this is constructed through the memory map utilities of the scl (see @ref gtry_scl_memorymaps_page).

The memory map can be serialized into an array of entries that can be used by the driver to look up register addresses:
```cpp
	auto memoryMapEntries = gtry::scl::exportAddressSpaceDescription(memoryMap.getTree().physicalDescription);
```

This array can be used directy (e.g. in simulation) or exported into C/C++ code for hardcoding addresses into a driver:
```cpp
	std::ofstream file("memory_map.generated.h", std::fstream::binary);
	gtry::scl::format(file, "memory_map", memoryMapEntries);
```
```cpp
#pragma once

#include <gatery/scl/driver/MemoryMapEntry.h>

namespace myNameSpace
{
	using gtry::scl::driver::MemoryMapEntry;
	#include "memory_map.generated.h"

	// use memory_map array, see below
}
```

The driver can now look up these entries either 1) *dynamically* from the `memoryMapEntries`, e.g. in simulation directly after circuit generation, or 2) *statically*, e.g. in the final actual driver, from the included, hardcoded `memory_map` array.
The latter under optimization compiles the lookups away into addresses.
This is handled through the `scl::driver::DynamicMemoryMap` and `scl::driver::StaticMemoryMap` classes which provide the same interface for both cases.

We will start with the static case. The `scl::driver::StaticMemoryMap` can be specialized with the `memory_map` array.
All instances of this `MyMemoryMap` class are equal and exchangeable and can be used to query the address hierarchy.
```cpp
	using MyMemoryMap = gtry::scl::driver::StaticMemoryMap<memory_map>;
```

The dynamic `scl::driver::DynamicMemoryMap`, seeking to emulate the same behavior, needs to be specialized in the same way to create a unique type whose instances are all equal.
To provide all instances with the dynamic `memoryMapEntries`, a static member function needs to be initialized:
```cpp
	struct MyMemoryMapUniquenessHelper { };
	using MyMemoryMap = scl::driver::DynamicMemoryMap<MyMemoryMapUniquenessHelper>;
	MyMemoryMap::memoryMap = scl::driver::MemoryMap(memoryMapEntries);
```

From now on, both cases behave the same. 
The hierarchy of the memory map can be descended into using symbolic names:
```cpp
	MyMemoryMap myMemoryMap{};
	auto aChannel = myMemoryMap.template get<"some_tilelink">().template get<"a">();
	auto valid = aChannel.template get<"valid">();
	auto payload = aChannel.template get<"payload">();
```

Each of these handles can be querried to retrieve information on the register or memory region:
```cpp
	size_t addressInBytes = valid.addr() / 8;
	size_t sizeInBits = valid.width();
```

Note that the template mechanism employed here allows in the `scl::driver::StaticMemoryMap` case for C++ to resolve all the symbolic name lookups in compilation time and thus hardcode the addr, width, ... directly into the driver code.

However, this also means that helper functions operating on a sub-region of a memory map must be templated functions:
```cpp
	template<IsStaticMemoryMapEntryHandle SubRegion>
	void doSomethingInteresting(SubRegion addr) {
		size_t addr = addr.template get<"random_register">().addr() / 8;
		// ...
	}

	// ...

	doSomethingInteresting(myMemoryMap.template get<"some_cool_thing">());
```

## Driver Access to Memory Maps

To abstract away the method by which writing to and reading from the memory map happens, all driver access to the registers must happen through the `gtry::scl::driver::MemoryMapInterface` interface.
```cpp
template<typename Payload, IsStaticMemoryMapEntryHandle Addr> requires (std::is_trivially_copyable_v<Payload>)
void writeToStream(gtry::scl::driver::MemoryMapInterface &interface, Addr streamLocation, const Payload &payload)
{
	while (interface.readUInt(streamLocation.template get<"valid">())) ;

	interface.write(streamLocation.template get<"payload">().addr()/8, payload);

	interface.writeUInt(streamLocation.template get<"valid">(), 1);
}
```

Helper functions for reading and writing common interfaces can be found in `libs/gatery/source/gatery/scl/driver/MemoryMapHelpers.h`, these currently consist of reading and writing streams and tile link interfaces, e.g.:
```cpp
	// Note that these are padded according to c++ rules.
	struct MyCommand {
		std::uint32_t a, b;
	} myCommand = { ...  };

	gtry::scl::driver::writeToStream(interface, addr.template get<"my_cmd">(), myCommand);
```
```cpp
	gtry::scl::driver::writeToStream(interface, addr.template get<"more_complex_stream">(), [&myData](MemoryMapInterface &interface, auto payload) {
		// This callback gets called when the stream's staging register is ready to be populated. `payload` refers to the payload field of the stream.
		interface.writeUInt(payload.template get<"address">(), myData.addr);
		interface.write(payload.template get<"data">().addr()/8, std::as_bytes(std::span{myData.data}));
	});
```
```cpp
template<IsStaticMemoryMapEntryHandle Addr>
void uploadToMemory(gtry::scl::driver::MemoryMapInterface &interface, Addr addr, size_t memoryStartAddr, std::span<const std::uint64_t> data) override
{
	gtry::scl::driver::writeToTileLink(interface, addr.template get<"memory_tilelink_interface">(), memoryStartAddr, std::as_bytes(data));
}
```

> The `gtry::scl::driver::MemoryMapInterface` still needs a bit more cleanup / consistency and I am
> looking for a way to combine it with the StaticMemoryMap, but for now this is a separate thing.

## Running the Driver in Simulation

To run driver code in simulation, bind the memory map's tile link interface (potentially using simulation overrides) to a tile link master model.

To write to the tile link master model, instantiate the `gtry::scl::driver::SimulationMapped32BitTileLink` class, which implements `gtry::scl::driver::MemoryMapInterface`.
```cpp
	auto fromMemoryMap = scl::toTileLinkUL(memoryMap, 32_b, width(scl::pci::TlpAnswerInfo{}));

	scl::TileLinkMasterModel linkModel;
	linkModel.init("cpuBus", 
		fromMemoryMap->a->address.width(), 
		fromMemoryMap->a->data.width(), 
		fromMemoryMap->a->size.width(), 
		fromMemoryMap->a->source.width());

	*fromMemoryMap <<= (scl::TileLinkUL&)linkModel.getLink();


	auto memoryMapEntries = gtry::scl::exportAddressSpaceDescription(memoryMap.getTree().physicalDescription);
	scl::driver::SimulationMapped32BitTileLink driverInterface(linkModel, clockOfMemoryMap);

	struct MyMemoryMapUniquenessHelper { };
	using MyMemoryMap = scl::driver::DynamicMemoryMap<MyMemoryMapUniquenessHelper>;
	MyMemoryMap::memoryMap = scl::driver::MemoryMap(memoryMapEntries);
```

With the interface and memory map in place, the driver code can execute and access the memory mapped interface.
However, since driver code knows nothing about being in a simulation (and isn't supposed to know about it), it is not written as coroutines (co_await, co_return, ...) but as regular, potentially blocking function calls.
This driver code must be run as a simulation fiber, with blocking (or otherwise simulation accessing) calls using the `sim::SimulationFiber::awaitCoroutine` function to transition into a coroutine:
```cpp
	addSimulationFiber([&driverInterface, &clock, this](){

		sim::SimulationFiber::awaitCoroutine<int>([&clock]()->SimFunction<int>{ 
			co_await OnClk(clock); // await reset
		});

		runDriverCode(driverInterface, MyMemoryMap{});

		stopTest();
	});
```

> I have not yet gotten around to writing a version of `sim::SimulationFiber::awaitCoroutine` for the return value `void` so just use `int` in those cases.

The `gtry::scl::driver::SimulationMapped32BitTileLink` class internally uses the same `sim::SimulationFiber::awaitCoroutine` mechanism to transition and use the tile link master model coroutines for driving the tile link bus to the memory map.

If the driver code requires multiple threads, spawn multiple simulation fibers, one for each thread.

> Note that simulation fibers are executed sequentially (using the coroutine calls as preemption points), so any race condition that they might have are not visible in simulation.
> Also note that simulation fibers execute while the simulation is paused, thus appearing to be infinitely fast compared to the circuit.

It is legal for simulation fibers to contain `while (true)` loops, similarly to simulation processes.
When the simulation stops, simulation fibers are terminated with their stack being unrolled by raising a `gtry::sim::SimulationFiber::SimulationTerminated` exception which must not be caught by the simulation fiber.

## Running the Driver on Linux for a PCIe Card

If the circuit is running on a PCIe card, the memory map is probably exposed to the host as part of it's physical address space.
Gatery provides tools for accessing it from a user space process.

Linux lists PCIe devices in the `/sys/bus/pci/devices/` folder, which can be enumerated:
```cpp
	gtry::scl::driver::lnx::PCIDeviceFunctionList devices;
	for (auto function : devices)
		std::cout << "Vendor: " << function.vendorId() << "  Device: " <<  function.deviceId() << "   Class: " << function.deviceClass() << std::endl;
```

The specific device and function in question can either be identified by its IDs, or by its location on the bus:
```cpp
	auto function = devices.findDeviceFunction(
						0x0815, // device Id
						0xCAFE, // vendor Id
						0  // function
					);
	// OR
	auto function = devices.findDeviceFunction(
						0, // domain
						1, // bus
						0, // device
						0  // function
					);
```
After a reboot, it usually needs to be enabled, after which its address space is can be accessed through memory mapping a resource file.
```cpp
	function.enable();
	// function.resource(0) is the path to the resource file to be memory mapped for the PCIe-device's Base Address Register 0.
```

The static memory map can be build as explained above:
```cpp
	using MyMemoryMap = gtry::scl::driver::StaticMemoryMap<memory_map>;
```
To write to the registers, the `gtry::scl::driver::lnx::UserSpaceMapped32BitEndpoint` class implements `gtry::scl::driver::MemoryMapInterface` and handles the memory mapping of the device's resource file as well as access to it.
This allows to run the exact same driver code as in the simulation:
```cpp
	// I know, this needs cleaning up.
	size_t memoryMapWidth = ((gtry::scl::driver::MemoryMapEntryHandle) MyMemoryMap{}).width() / 8;
	gtry::scl::driver::lnx::UserSpaceMapped32BitEndpoint driverInterface(function, memoryMapWidth);

	runDriverCode(driverInterface, MyMemoryMap{});
```

## Future Work

- Probably move things like logging, sleeping, and waiting for device reset into the `gtry::scl::driver::MemoryMapInterface` (or a similar driver interface) as well?
- Lots of cleaning up and polishing
- Host ram buffers for device DMA access
- IOMMU


See @ref gtry_scl_driver

------------------------------

@defgroup gtry_scl_driver User Space Driver Framework
@ingroup gtry_scl
See @ref gtry_scl_driver_page
