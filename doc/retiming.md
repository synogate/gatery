@page gtry_frontend_retiming_page Retiming

Gatery has support for register retiming: Shifting registers over logic.
This retiming only happens in few, select cases and is controlled by the designer.

## Forward Register Retiming

The purpose of forward retiming is to move registers forward through a pipeline.
Gatery can push these registers not only through logic, but also through other registers and state machines if certain conditions are met.

The typical application are situations, where the manual placement of correctly balanced pipeline registers is cumbersome and error prone, but can be more easily done at a different location e.g. at the start of the pipeline.


### Spawning Registers

Registers for forward retiming can be spawned at predetermined locations, thus actually changing the observable behavior of the design.
Since signals need to be kept in balance, these spawning locations group multiple signals together such that if a register is spawned on one of them, balancing registers are spawned on all of them.

```cpp
using namespace gtry;

UInt someComputation(UInt a, UInt b, UInt c)
{
	PipeBalanceGroup group;

	a = pipeinput(a, group);
	b = pipeinput(b, group);
	c = pipeinput(c, group);

	//...
}
```
Or, as an equivalent shortand:
```cpp
using namespace gtry;

UInt someComputation(UInt a, UInt b, UInt c)
{
	gtry::pipeinputgroup(a, b, c);
	//...
}
```

### Retiming Manual Registers Forward

Manually placed registers are by default not moved by retiming (but my be jumped-over by other registers if enable conditions are compatible).
They can be made movable in which case they will be picked up by retiming as well:

```cpp
	a = reg(a, { .allowRetimingBackward = true });
```

> While manual registers can be used in retiming, at the moment retiming does require the use of the `pipeinputgroup` on the input signals.

### Retiming Registers

To retime a register to a certain location, place a retiming hint:
```cpp
UInt someComputation(UInt a, UInt b, UInt c)
{
	pipeinputgroup(a, b, c);

	// potential other operations on a, b, c

	return pipestage(a + b) + c;
}
```

The `pipestage` call itself can be seen as an identitfy function.
Retiming will (attempt to) move a register to the location labeled by `pipestage`, but will also place balancing registers, in this case on the `c` input, such that the behavior remains unchanged.
Only the `PipeBalanceGroup`/`pipeinputgroup(...)` spawning new registers has an actual outside observable behavior change.

Calls to `pipestage` can be repeated to introduce multiple pipeline stages:
```cpp
UInt someComputation(UInt a, UInt b, UInt c)
{
	pipeinputgroup(a, b, c);

	// potential other operations on a, b, c

	b = pipestage(b);

	// more operations

	return pipestage(pipestage(a + b) + c);
}
```

> Note that contrary to manually calling `reg`, the retiming algorithm will ensure that `a`, `b`, and `c` will remain balanced, and that no operation will see values on `a`, `b`, and `c` from different points in time.

### Enable Conditions

Often, pipelining register have to be enabled based on e.g. a ready signal in order to stall a pipeline.

The enable of retimed registers is inferred from the registers that are moved forward (or spawned), thus allowing the code that places `pipestage` to be fully unaware of the pipeline it is integrated into and it's enable signals.
In order to stall a pipeline with register retiming, the register spawner must be conditionally enabled with the stalling signal:

```cpp
UInt someComputation(UInt a, UInt b, UInt c)
{
	return pipestage(pipestage(a + b) + c);
}

UInt pipelinedComputation(UInt a, UInt b, UInt c, Bit enable)
{
	ENIF (enable)
		pipeinputgroup(a, b, c);

	return someComputation(a, b, c);
}
```

Note that if the circuit contains registers e.g. for state, these registers enable must be a conjunction of the pipeline enable signal and potential other terms in order for register retiming to be possible over these state registers:

```cpp
UInt someComputation(UInt a, UInt b)
{
	UInt counter = 32_b;
	counter = reg(counter + 1);

	return pipestage(pipestage(a + b) + counter);
}

UInt pipelinedComputation(UInt a, UInt b, Bit enable)
{
	ENIF (enable) {
		pipeinputgroup(a, b);

		// Call to someComputation must be in the enable scope in order to stall the counter register.
		return someComputation(a, b);
	}
}
```

### External Nodes and Enable Conditions

External nodes can contain registers as well.
To enable correct retiming over external nodes, their enable signals must be flagged as such and must follow the same conjunction rule as the enable signals of other manually placed registers:

```cpp
	ExternalModule fluxCapacitor{ "FluxCapacitor", "FUTURSIM", "imagcomponents" };
	fluxCapacitor.in("a", op1.width()) = (BVec) op1;
	fluxCapacitor.in("b", op2.width()) = (BVec) op2;
	fluxCapacitor.in("enable", { .isEnableSignal = true }) = EnableScope::getFullEnable();
	result = (UInt) fluxCapacitor.out("O", result.width());
```

### Negative Registers

Register retiming allows for a virtual concept in gatery called negative registers.
While a normal register ingests a signal and at its output yields the value of the signal on the previous cycle, a negative register yields the value of the signal on the next/future cycle.
This prediction is obviously not actually happening.
Rather, retiming is used to delay everything but the output of the negative register, thus creating the appearence of predicting the future.

The foremost application of negative registers is in combination with external nodes.
Often, external nodes such as DSPs feature pipelining registers, requiring concious thought about signal delays and balancing registers.
By combining such external nodes with the appropriate amount of negative registers, they can be conceptually turned into combinatorical circuits.
This greatly simplifies working with them, while the register retiming handles placement of balancing registers.

Negative registers are placed through a call to `negativeReg`.
The function returns the future value of its input as well as an enable signale.
The enable signal is the enable of the piepeline, more precisely of the register that would have been placed at that location if the call to `negativeReg` was a call to `pipestage` instead.
Its is provided in case the negative register is used to compensate a register in an external node, in which case the enable must be provided to the external node.
```cpp
	auto [a_next, enable] = negativeReg(a);
```


A practical application might look like the following:

```cpp
UInt dspAdd(UInt op1, UInt op2)
{
	// Combinatorical simulation circuit
	UInt result = op1 + op2;

	// Hard-Macro with 1-cycle delay, made combinatorical with negative register.
	Bit enable;
	ExternalModule hardAdder{ "SuperDuperAdder", "UNISIM", "vcomponents" };
	hardAdder.in("a", op1.width()) = (BVec) op1;
	hardAdder.in("b", op2.width()) = (BVec) op2;
	hardAdder.in("enable", { .isEnableSignal = true }) = enable & EnableScope::getFullEnable();
	UInt exportResult = (UInt) hardAdder.out("O", result.width());
	std::tie(exportResult, enable) = negativeReg(exportResult);

	result.exportOverride(exportResult);
	return result;
};

UInt pipelinedAdd(UInt a, UInt b, UInt c, Bit enable)
{
	ENIF (enable) {
		pipeinputgroup(a, b, c);

		UInt a_plus_b = dspAdd(a, b);
		return dspAdd(a_plus_b, c); // The balancing register on c is added automatically by the register retiming
	}
}
```


