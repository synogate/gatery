#pragma once

#include <gatery/frontend.h>
#include <gatery/frontend/GHDLTestFixture.h>
#include <gatery/simulation/UnitTestSimulationFixture.h>
#include <gatery/frontend/FrontendUnitTestSimulationFixture.h>
#include <gatery/simulation/waveformFormats/VCDSink.h>

#include <gatery/hlim/supportNodes/Node_SignalGenerator.h>
#include <gatery/utils.h>

#include <gatery/scl/crypto/sha1.h>
#include <gatery/scl/crypto/sha2.h>
#include <gatery/scl/crypto/md5.h>
#include <gatery/scl/crypto/SipHash.h>
#include <gatery/scl/crypto/TabulationHashing.h>
#include <gatery/scl/driver/driver_utils.h>
#include <gatery/scl/io/HDMITransmitter.h>
#include <gatery/scl/io/uart.h>
#include <gatery/scl/utils/BitCount.h>
#include <gatery/scl/utils/Thermometric.h>
#include <gatery/scl/utils/OneHot.h>
#include <gatery/scl/kvs/TinyCuckoo.h>
#include <gatery/scl/Fifo.h>



#include <gatery/scl/arch/intel/IntelDevice.h>
#include <gatery/scl/arch/intel/ALTPLL.h>
#include <gatery/scl/riscv/DualCycleRV.h>
#include <gatery/scl/riscv/ElfLoader.h>
#include <gatery/scl/synthesisTools/IntelQuartus.h>
#include <gatery/scl/tilelink/tilelink.h>
#include <gatery/scl/tilelink/TileLinkMasterModel.h>
#include <gatery/scl/tilelink/TileLinkAdapter.h>
#include <gatery/scl/tilelink/TileLinkHub.h>
#include <gatery/scl/tilelink/TileLinkStreamFetch.h>
#include <gatery/scl/TransactionalFifo.h>
#include <gatery/scl/io/ddr.h>
#include <gatery/scl/io/usb/Ulpi.h>
#include <gatery/scl/io/usb/GpioPhy.h>
#include <gatery/scl/io/usb/Function.h>
#include <gatery/scl/io/usb/CommunicationsDeviceClass.h>
#include <gatery/scl/io/SpiMaster.h>
#include <gatery/scl/memory/sdram.h>
#include <gatery/scl/memory/SdramControllerSimulation.h>
#include <gatery/scl/memory/MemoryTester.h>
#include <gatery/scl/memoryMap/MemoryMap.h>
#include <gatery/scl/memoryMap/MemoryMapConnectors.h>
#include <gatery/scl/memoryMap/PackedMemoryMap.h>
#include <gatery/scl/memoryMap/TileLinkMemoryMap.h>
#include <gatery/scl/Adder.h>
#include <gatery/scl/stream/metaSignals.h>
#include <gatery/scl/stream/Packet.h>
#include <gatery/scl/stream/SimuHelpers.h>
#include <gatery/scl/stream/Stream.h>
#include <gatery/scl/stream/StreamArbiter.h>
#include <gatery/scl/stream/StreamBroadcaster.h>
#include <gatery/scl/stream/StreamConcept.h>
#include <gatery/scl/stream/StreamDemux.h>
#include <gatery/scl/stream/streamFifo.h>
#include <gatery/scl/stream/StreamRepeatBuffer.h>

#include <gatery/scl/Counter.h>
#include <gatery/scl/cdc.h>
#include <gatery/scl/Fifo.h>
#include <gatery/scl/TransactionalFifo.h>