@page gtry_frontend_clocks_resets_page Clocks and Resets

# Intro

In Gatery, clocks and resets are not handled via regular logic and signals but are a dedicated concept.
A `Clock` in gatery incapsulates the concept of the clock signal, its reset, its properties such as frequency and phase, but also reset types and register and memory initialization settings.

Clocks can inherit from other clocks, which allows to create a tree of clocks where child clocks can override parent properties. This is useful for creating related (e.g. phase ealigned) clocks, but also for creating a new `Clock` with different properties but that actually uses the same clock signal as the parent clock.

# Creating a Clock

A clock is created by instantiating the forntend `Clock` class. The constructor takes a `ClockConfig` object that defines the properties of the clock.
```cpp
#include <gatery/clock.h>
#include <gatery/simulation.h>

using namespace gtry;
using namespace gtry::frontend;

Clock clock({
	.absoluteFrequency = 100'000'000,
	.name = "clk",
	.resetName = "rst",
	// ... other properties
});
```

To inherit from a parent clock, use the `deriveClock` function.
```cpp
Clock derivedClock = parentClock.deriveClock({
	.name = "derived_clk",
	.resetName = "derived_rst",
	.frequencyMultiplier = 2,
	.initializeMemory = false,
	// ... other properties
});
```

These clock instances can then be used to drive clocked entities such as register:
```cpp
Bit b = reg(a, '0', { .clock = clock });
```

# Clock Scopes

To prevent having to pass `Clock` instances around explicitely to every clocked entity of a circuit, a clock scope can be defined.
```cpp
CLock clock(...);
ClockScope clockScope(clock);

Bit b = reg(a, '0'); // Automatically uses the clock from the scope
```

Clock scopes can be nested, and the innermost scope will be used for all clocked entities that do not have a clock passed explicitely.
```cpp
Clock clock(...);
Clock derivedClock = clock.deriveClock( ... );

ClockScope clockScope(clock);
Bit b1 = reg(a1, '0'); // Automatically uses `clock`
{
	ClockScope clockScope2(derivedClock);
	Bit b2 = reg(a2, '0'); // Automatically uses `derivedClock`
}
Bit b3 = reg(a3, '0'); // Automatically uses `clock`
```

Clock scopes also work through function calls:
```cpp
Bit foo(Bit a) {
	return reg(a, '0');
}
// ...

	ClockScope clockScope(clock);
	Bit b = foo(a); // Automatically uses `clock`
```

# Automatic vs Manual Clocks

By default, clocks in gatery are handled automatically by the simulator.
This means that the simulator will automatically drive the clock signal according to the specified frequency.
It will also automatically handle the reset signal and assert it for the required duration (may be longer than one clock cycle for memory initialization).
Upon export to VHDL, these clocks will result in top level clock and reset port map signals.

Alternatively, clocks can be handled manually. This means that the simulator will not drive the clock signal and the user has to do this explicitely through logic signals.

## Manually Driving Clocks

To manually drive a clock, override the clock signal. In this case, the simulator will not automatically drive the clock signal.
```cpp
Bit clockSignal = ...; // Drive it somehow

Clock clock({ .absoluteFrequency = 10'000'000 });
clock.overrideClkWith(clockSignal);
```

It is legal to connect `clockSignal` to an input pin and have it driven by a simulation process.

It is still useful to provide a frequency as this will be passed on as a timing constraint to the synthesis tools.
When manually driving a clock, the reset signal should also be driven manually.

## Manually Driving Resets

To manually drive a reset, override the clock reset signal. In this case, the simulator will not automatically drive the reset signal and the logic is responsible for asserting the reset signal for a sufficient duration.
```cpp
Bit resetSignal = ...; // Drive it somehow

// ...
clock.overrideRstWith(resetSignal);
```

It is legal to connect `resetSignal` to an input pin and have it driven by a simulation process.

Currently, there are no checks in place to ensure that the reset signal is asserted for a sufficient duration.

## Simulation-Only Automatic Clocks

Sometimes, a clock is driven by an external blackbox module, but for the simulation the blackbox is to be omitted in which case it is convenient to have the clock driven automatically.

For this purpose, logic driven clock signals can have export overrides.
If a logic clock signal has an export override that sources it e.g. from a black box (or some other logic), but the simulation-branch of that export override is unconnected, then the clock is considered an automatic clock in the simulation but will be tied to the blackbox in the export.
```cpp
Bit clockSignal; // Unconnected signal
clockSignal.exportOverride( myBlackBox.clkSignal() ); // Only driven in export

Clock clock({ .absoluteFrequency = 10'000'000 });
clock.overrideClkWith(clockSignal); // Will use `myBlackBox.clkSignal()` in export, but run automatically with 10 MHz in simulation
```

# Using Clock and Reset Signals in Logic

Clock and reset signals can be extracted and used in logic just like any other signal.
```cpp
Bit clk = clock.clkSignal();
Bit rst = clock.rstSignal();

Bit ddr = mux(clk, {a, b});
```
This is valid for both, automatic and manual clocks.

It is illegal to build combinatorial loops through `clkSignal` and `overrideClkWith` (and their reset counterparts), though it is currently not checked for.

# Clock Domain Crossing

Gatery does not allow implicit clock domain crossings.
A signal gets associated with a clock if it is driven by a clocked entity, e.g. register, memory, or a pin (more on that later).
Signals associated with a clock (i.e. in that clock domain) can not be mixed with signals from another clock domain and can not be consumed by a clocked entity that is not in the same clock domain.

As an example, the following is **not allowed**:
```cpp
// Assuming clock1 and clock2 are different clocks and not derived in a compatible manner from each other

Bit b1 = reg(a1, { .clock = clock1 });
Bit b2 = reg(a2, { .clock = clock2 });

pinOut(b1 ^ b2); // Error: b1 and b2 are in different clock domains
```

Also **not allowed**:
```cpp
// Assuming clock1 and clock2 are different clocks and not derived in a compatible manner from each other

Bit b = reg(a, { .clock = clock1 });
Bit c = reg(b, { .clock = clock2 }); // Error: b is in clock1 domain, but this reg is in clock2 domain
```

## Clock Domain of Pins

Pins in gatery also have a clock domain.
Output pins simply inherit the clock domain of the signal they are driven from.
Input pins derive their clock domaing from the clock scope of when they are created.

This allows to extend the clock domain crossing checks to pins as well.
```cpp
    {
		// Pins of memory port A
		ClockScope clockScope(clockPortA);
		writeDataA = pinIn(8_b).setName("wrDataA");
		// ...
	}
    {
		// Pins of memory port B
		ClockScope clockScope(clockPortB);
		writeMaskB = pinIn(8_b).setName("wrMaskB");
		// ...
	}

	BVec maskedData = writeDataA & writeMaskB; // Error: writeDataA and writeMaskB are in different clock domains
```

## Forcing Clock Domain Crossings

To force a clock domain crossing, use the `allowClockDomainCrossing` function to indicate that you know what you are doing and are aware of the involved clocks:
```cpp
// Assuming clock1 and clock2 are different clocks and not derived in a compatible manner from each other
Bit b_clock1 = reg(a, { .clock = clock1 });
Bit b_clock2 = allowClockDomainCrossing(b_clock1, clock1, clock2); // Forces the crossing from clock1 to clock2
Bit c = reg(b_clock2, { .clock = clock2 });
```

Note that `allowClockDomainCrossing` is a no-op in simulation as well as export.
Additional synchronizers etc. must be build to ensure that the crossing is safe.
Gatery has a number of CDC primitives in the SCL that should be used, such as CDC-fifos.


The `allowClockDomainCrossing` function can be given an optional `CdcNodeParameter` configuration to provide additional information for constraint file generation.