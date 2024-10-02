@page gtry_frontend_logging_page Logging

# Reporting and Logging in Gatery

When generating a circuit, Gatery produces logging messages and information, especially in the case of errors.
This information is directed at a common logging interface, to which different backends can be attached.

## HTML Reporting

Reporting can be activated by switching the logging backend to the html reporting:
```cpp
#include <gatery/debug/DebugInterface.h>

...
gtry::dbg::logHtml("output/path/to/html/report/")
```
This needs to be done before whatever is to be logged happens, and thus should be one of the first things to do. The call creates the given folder and places a couple of html and related files in it, amongst which the `index.html` is the starting point. The log is supposed to be fully functional during generation and especially if the generation is aborted e.g. in a failure.

## Reporting in Unit Tests

### HTML Reporting in Unit Tests

For the the unit test fixture, reporting can be activated through command line arguments, e.g.:
```bash
./unittest_frontend -- --report html
```
Will run all unit tests and will create one report folder for each test, prefixed by the test name.

### WebDebugger in Unit Tests

For individual tests, the same command line switch can be used to activate the web debugger that offers dbugging information via a websocket server.
```bash
./unittest_frontend --run_test=my_test -- --report web
```

Note that this suspends execution of the unittest until the webdebugger is connected. It will also halt execution at the end of the unit test to allows inspection from the web frontend, thus making only useful for individual tests.


# Writing Log Messages

The most important type of information handed of to the logging framework is logging messages.
These are created by passing `gtry::dbg::LogMessage`s to `gtry::dbg::log(.)`:

```cpp
#include <gatery/debug/DebugInterface.h>

using namespace gtry::dbg;

log(LogMessage("Hello World!"));
```

The `gtry::dbg::LogMessage` class is a helper class to compose complex logging messages from multiple parts in a fashion that is very similar to how `std::ostream` works:

```cpp
using namespace gtry::dbg;

log(LogMessage() << "There are " << 3 << " parts to this message.");
```

These message parts can not just be strings and numbers, but also entities of the circuit.
This allows the backend to take additional information into account when rendering the message, such as cross linking, rendering images, etc.


```cpp
using namespace gtry::dbg;

log(LogMessage() << "There are is something wrong with the node " << myNode << " it should be in group " << group << " but actually it is in " << myNode->getGroup());
```

Often times, it is very helpful in diagnosing a problem when a visualization of (part of) the graph is provided.
For this, an arbitrary subnet of nodes can be added to the message which the logging backend can render or display in some fashion:

```cpp
using namespace gtry::dbg;
using namespace gtry::hlim;

Subnet relevantNodes = Subnet::fromNodeGroup(myNodeGroup, true);

log(LogMessage() << "There are is something very wrong in " << myNodeGroup << " take a look at it: " << relevantNodes);
```




Additional special message parts allow setting flags on which messages might be filtered when presented to the viewer, such as severity, step in the pipeline, or which part of the hierarchy this message refers to:


```cpp
using namespace gtry::dbg;

log(LogMessage() << LogMessage::LOG_INFO << LogMessage::LOG_POSTPROCESSING << "This is some information");
log(LogMessage() << LogMessage::LOG_WARNING << LogMessage::LOG_POSTPROCESSING << "Something has happened that can be handled but is probably resulting in a broken circuit");
log(LogMessage() << LogMessage::LOG_ERROR << LogMessage::LOG_POSTPROCESSING << "Some operation failed.");

Area myArea("test", true);
log(LogMessage() << LogMessage::Anchor(myArea.getNodeGroup()) << "Some important information that relates to the area that I'm building right now");
```



See @ref gtry_frontend_logging

------------------------------

@defgroup gtry_frontend_logging Logging
@ingroup gtry_frontend
See @ref gtry_frontend_logging_page
