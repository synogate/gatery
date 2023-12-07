#pragma once

#include <gatery/frontend.h>
#include <gatery/frontend/GHDLTestFixture.h>
#include <gatery/simulation/UnitTestSimulationFixture.h>
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
#include <gatery/scl/utils/OneHot.h>
#include <gatery/scl/kvs/TinyCuckoo.h>
