@page gtry_frontend_simulation_processes_page Simulation Processes


# Reading and Driving IO-pins

## Emulating Pull-up & Pull-Down Behavior

When creating tristate or input pins, these can be configured to have a virtual pull-up or pull-down resistor that defines the default value that the circuit observes on the pin if it is not driven.
This is controlled through the `highImpedanceValue` field in the `PinNodeParameter`:
```cpp
	auto normalInputPin = pinIn(8_b, { .highImpedanceValue = HighImpedanceValue::PULL_UP }); // Default to all bits '1'
	auto tsPin = tristate(value, enable, { .highImpedanceValue = HighImpedanceValue::PULL_DOWN }); // Default to all bits '0'
```

If nothing is specified explicitely, the default is to read undefined:
```cpp
	auto tsPin = tristate(value, enable, { .highImpedanceValue = HighImpedanceValue::UNDEFINED }); // Default to all bits 'x'
```

A pin is not explicitely driven, if neither a simulation process is driving it, nor the circuit (i.e. input only pin, or tristate pin with enable deasserted).

Once a simulation process started driving a pin, it can stop driving by assigning a literal with the high-impedance 'z' value:
```cpp
	// In simulation process
	// Assuming tsPin not being driven by circuit

	// Start driving all bits
	simu(tsPin) = "xCAFFE";
	// ...

	// Stop driving bits 4-11
	simu(tsPin) = "xCAzzE";
```

A shorthand for not driving any bits exists that is the equivalent of assigning 'z' to all bits.
```cpp
	// In simulation process
	// Assuming tsPin not being driven by circuit

	simu(tsPin) = "xCAFFE";
	// ...
	simu(tsPin).stopDriving();
```

Do not that this is not to be confused with `invalidate` which will continue driving, but with undefined values:
```cpp
	// In simulation process
	// Assuming tsPin not being driven by circuit

	simu(tsPin).invalidate(); // Forces the pins to undefined, independent of `highImpedanceValue`
	// ...
	simu(tsPin).stopDriving();  // Pins go to value defined in `highImpedanceValue`
```
