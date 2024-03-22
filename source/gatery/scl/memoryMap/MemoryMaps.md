@page gtry_scl_memorymaps_page Memory Maps

Gatery supports the automatic creation of memory mapped registers.
Gatery will handle the actual register creation, address space allocation, and address decoding.

> This (new) implementation is still experimental and is being rushed out of the door.
> Some interfaces will still change!

## Basics

The `gtry::scl::MemoryMap` interface, provides a general interface to which signals can be registered.
On its own, performs none of the above things and can be used as a placeholder when, due to configuration, no actual memory mapped registers are desired.
The `gtry::scl::PackedMemoryMap` implements a memory map that performs address space allocation and automatically splits registered signals if their width exceeds the master's bus width.

Signals are not registered via member functions of `gtry::scl::MemoryMap`, but through free standing functions that function analogous to `gtry::pinIn` and `gtry::pinOut`:
```cpp

#include <gatery/scl/memoryMap/PackedMemoryMap.h>
#include <gatery/scl/memoryMap/MemoryMapConnectors.h>

// ...

	UInt signalToCircuit;
	Bit signalFromCircuit;

	gtry::scl::PackedMemoryMap memoryMap;
	gtry::scl::mapIn(memoryMap, signalToCircuit, "signalToCircuit");
	gtry::scl::mapOut(memoryMap, signalFromCircuit, "signalFromCircuit");

```

It is legal to `gtry::scl::mapIn` and `gtry::scl::mapOut` the same or two different signals to the same name.
In this case, they are bound to the same memory address, which also allows reading and writing to the same address to be directed to two different signals.


To actually memory map the signals and allow access, the memory map must be connected to a stream.
So far, only a `gtry::scl::TileLinkUL` adapter is provided in the form of the `gtry::scl::toTileLinkUL` function which returns a `scl::Reverse<gtry::scl::TileLinkUL>` that can be connected to a tile link master:
```cpp

// ...

	gtry::scl::PackedMemoryMap memoryMap("memoryMap");
	gtry::scl::mapIn(memoryMap, signalToCircuit, "signalToCircuit");
	gtry::scl::mapOut(memoryMap, signalFromCircuit, "signalFromCircuit");

	BitWidth desiredBusWidth = 8_b;
	auto reverseTileLink = toTileLinkUL(memoryMap, desiredBusWidth);

	pinIn(*reverseTileLink, "cpu_tilelink");

```

Address allocation will in the future be propagated as meta data through the tile link struct, but can already be querried from the memory map directly after the creation of the tile link slave:
```cpp
	std::cout << memoryMap.getTree().physicalDescription << std::endl;
```

They can also be exported as a filter file for GTKWave to translate numerical addresses on the tile link bus to symbolic register names:
```cpp
	auto memoryMapEntries = gtry::scl::exportAddressSpaceDescription(memoryMap.getTree().physicalDescription);

	std::ofstream file("myCircuitsMemoryMap.gtkwave.filter", std::fstream::binary);
	gtry::scl::writeGTKWaveFilterFile(file, memoryMapEntries);
```

## Compounds

Compound signals can be mapped as well as long as they have been annotated (same as for `gtry::setName` or `gtry::pinIn`/`gtry::pinOut`).
They will not be `gtry::pack`ed, but rather have their members registered individually and recursively.
The following:
```cpp
struct MyStruct {
	BOOST_HANA_DEFINE_STRUCT(MyStruct,
		(Bit, field1),
		(BVec, field2),
		(BVec, field3)
	);
};

// ...

	MyStruct myStruct{ Bit{}, 4_b, 16_b };
	PackedMemoryMap memoryMap("memoryMap");
	mapIn(memoryMap, myStruct, "myStruct");
	mapOut(memoryMap, myStruct, "myStruct");

	toTileLinkUL(memoryMap, 8_b);
	std::cout << memoryMap.getTree().physicalDescription << std::endl;
```
produces the following layout (all offsets in bits):
```
 From: 0 size 32b
    Name: memoryMap
    Children: 
         From: 0 size 32b
            Name: myStruct
            Children: 
                 From: 0 size 1b
                    Name: field1
                 From: 8 size 4b
                    Name: field2
                 From: 16 size 8b
                    Name: field3_bits_0_to_8
                 From: 24 size 8b
                    Name: field3_bits_8_to_16
```

### Reverse

`gtry::Reverse` reverses the direction of members as is the case with `gtry::pinIn`/`gtry::pinOut`.
A reversed member of a compound that is `gtry::scl::mapIn`-ed is actually `gtry::scl::mapOut`-ed and vice versa.

## Annotation

> This part is not yet working properly, nor am I particularly happy with it, so this is more of an outlook.

In order to produce documentation for the generated address layout, compounds can be annotated with documentation through the `gtry::CompoundAnnotator` type trait.

If the template specialization of `gtry::CompoundAnnotator` for a given type provides a static `gtry::CompoundAnnotation annotation` (can be checked via `gtry::IsAnnotated`) then this documentation is picked up for the address layout description.

```cpp
template<>
struct gtry::CompoundAnnotator<MyStruct> {
	static gtry::CompoundAnnotation annotation;
};

gtry::CompoundAnnotation gtry::CompoundAnnotator<MyStruct>::annotation = {
		.shortDesc = "Short description",
		.longDesc = "Longer description",
		.memberDesc = {
			CompoundMemberAnnotation{ .shortDesc = "example field 1" },
			CompoundMemberAnnotation{ .shortDesc = "example field 2" },
			CompoundMemberAnnotation{ .shortDesc = "example field 3" },
		}
	};
```

The function `gtry::getAnnotation` returns a pointer to the `gtry::CompoundAnnotation` of a type if `gtry::IsAnnotated` or `nullptr` otherwise.

## Special Overloads

Some compounds/classes require specialized handling.
Before "blindly" mapping all members of a struct, the `gtry::scl::CustomMemoryMapHandler` type trait is invoked to check if a custom handler was created for that type (see `gtry::scl::HasCustomMemoryMapHandler`).
If the template specialization for a type implements a static `memoryMap` function, that function is invoked instead of the default reccursive member registration.
```cpp
	template<>
	struct CustomMemoryMapHandler<MyStruct> {
		static void memoryMap(MemoryMapRegistrationVisitor &v, MyStruct &signal, bool isReverse, std::string_view name, const CompoundMemberAnnotation *annotation) {
		}
	};
```

### Stream Overload

A special overload was implemented for `gtry::scl::strm::Stream`.
When registration reaches a stream, all members are registered as per their direction (i.e. ready in reverse), but if present, the ready or valid registers are augmented with special functionality.

For `gtry::scl::mapIn`-ed streams, i.e. streams being driven by the bus master, the payload is exposed as writeable addresses.
A transfer can be prepared by populating these fields through multiple writes.
The transfer is then initiated by writing `1` to the address of the valid field.
This valid register *automatically deasserts* upon transfer (e.g. when the receiver asserts ready).
The valid register is also readable, allowing the bus master to poll and check for its deassertion before preparing the next transfer.

For `gtry::scl::mapOut`-ed streams, i.e. streams being consumed by the bus master, the roles are reversed.
The bus master awaits data by polling the valid register until it becomes asserted.
Once data is available, it can be read from the payload addresses, potentially through multiple requests.
With the data consumed, the bus master signals the transfer by writing `1` to the address of the ready register.
This one *automatically deasserts* upon transfer (e.g. when the sender asserts valid).


See @ref gtry_scl_memorymaps

------------------------------

@defgroup gtry_scl_memorymaps Memory Maps
@ingroup gtry_scl
See @ref gtry_scl_memorymaps_page
