/*  This file is part of Gatery, a library for circuit design.
Copyright (C) 2021 Michael Offel, Andreas Ley

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
*/
#include "gatery/scl_pch.h"
#include "dma.h"
#include <gatery/scl/memoryMap/MemoryMapConnectors.h>



namespace gtry::scl {
	void tileLinkToAxiDMA(RvStream<TileLinkStreamFetch::Command>&& fetchCmd, RvStream<AxiToStreamCmd>&& depositCmd, TileLinkUB&& dataSource, Axi4& dataDest)
	{
		Area ent{ "scl_tileLink_to_axi_dma", true };
		RvStream<BVec> dataStream(dataSource.a->data.width());

		auto dataSourceMaster = TileLinkStreamFetch{}.enableBursts(depositCmd->bytesPerBurst * 8).generate(fetchCmd, dataStream, 0_b);
		dataSource <<= dataSourceMaster;
		HCL_NAMED(dataStream);

		axiFromStream(move(depositCmd), regDownstream(move(dataStream)), dataDest);
	}


	struct AxiDmaControl {
		scl::RvStream<scl::AxiToStreamCmd> depositCmd;
		scl::RvStream<scl::TileLinkStreamFetch::Command> fetchCmd;
		gtry::Reverse<scl::AxiTransferReport> axiReport;
	};

	void createDma(MemoryMap& map, TileLinkUB&& dataSource, Axi4& dataDest, BitWidth beatsW, size_t bytesPerBurst) {
		Area ent{ "scl_memory_mapped_dma", true };
		
		AxiDmaControl dmaControl;

		*dmaControl.depositCmd = scl::AxiToStreamCmd {
				.startAddress = dataDest.config().addrW,
				.endAddress = dataDest.config().addrW,
				.bytesPerBurst = bytesPerBurst,
		};

		*dmaControl.fetchCmd = scl::TileLinkStreamFetch::Command {
			.address = dataSource.a->address.width(),
			.beats = beatsW,
		};

		auto paddedHbmAxi = padWriteChannel(dataDest, dataSource.a->data.width());
		HCL_NAMED(paddedHbmAxi);

		tileLinkToAxiDMA(
			regDecouple(dmaControl.fetchCmd),
			regDecouple(dmaControl.depositCmd),
			tileLinkRegDecouple(move(dataSource)),
			paddedHbmAxi);

		dmaControl.axiReport = axiTransferAuditor(paddedHbmAxi, bytesPerBurst * 8_b);
		
		scl::mapIn(map, dmaControl, "dma_ctrl");
	}
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::AxiDmaControl, depositCmd, fetchCmd, axiReport);



