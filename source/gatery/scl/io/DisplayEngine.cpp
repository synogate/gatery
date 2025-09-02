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
#include "DisplayEngine.h"
#include "../arch/colognechip/io.h"
#include "../arch/colognechip/SpecialFunction.h"
#include "../stream/strm.h"
#include "../ShiftReg.h"
#include "../tilelink/TileLinkStreamFetch.h"
#include "../memoryMap/MemoryMapConnectors.h"


namespace gtry::scl
{
	RvStream<ColorRGB> displayColorTable(RvStream<UInt> index, MemoryMap& ctrl, const ColorRGB& blueprint)
	{
		Area ent{ "scl_displayColorTable", true };
		HCL_NAMED(index);

		Memory<ColorRGB> colorTable(index->width(), blueprint);
		colorTable.fillPowerOnState(
			displayColorTableDefaultValue(index->width(), blueprint.r.width(), blueprint.g.width(), blueprint.b.width())
		);

		mapIn(ctrl, colorTable, "color");
		return lookup(move(index), colorTable);
	}

	sim::DefaultBitVectorState displayColorTableDefaultValue(BitWidth indexW, BitWidth rW, BitWidth gW, BitWidth bW)
	{
		sim::DefaultBitVectorState initialTable;
		initialTable.resize(indexW.count() * (rW + gW + bW).bits());
		initialTable.setRange(sim::DefaultConfig::DEFINED, 0, initialTable.size());
		for (size_t i = 0; i < indexW.count(); ++i)
		{
			double brightness = pow(double(i) / indexW.count(), 2.2);
			size_t r = size_t(round(brightness * rW.count()));
			size_t g = size_t(round(brightness * gW.count()));
			size_t b = size_t(round(brightness * bW.count()));

			size_t entryOffset = i * (rW + gW + bW).bits();
			size_t rOffset = entryOffset + 0;
			size_t gOffset = rOffset + rW.bits();
			size_t bOffset = gOffset + gW.bits();

			initialTable.insert(sim::DefaultConfig::VALUE, rOffset, rW.bits(), r);
			initialTable.insert(sim::DefaultConfig::VALUE, gOffset, gW.bits(), g);
			initialTable.insert(sim::DefaultConfig::VALUE, bOffset, bW.bits(), b);
		}
		return initialTable;
	}

	std::deque<BVec> displayOpenLDIEncode(ColorRGB pixel, Bit de, Bit vsync, Bit hsync)
	{
		Area ent{ "scl_displayOpenLDIEncode", true };
		HCL_DESIGNCHECK(pixel.r.width() == pixel.b.width() && pixel.r.width() == pixel.g.width());
		HCL_DESIGNCHECK(pixel.r.width() == 6_b || pixel.r.width() == 8_b);

		std::deque<BVec> out(pixel.r.width() > 6_b ? 4 : 3, ConstBVec(7_b));

		UInt lvds = ConstUInt(out.size() * 7_b);
		lvds.lower(3 * 7_b) = cat(
			de, vsync, hsync,
			pixel.b.lower(6_b) & de,
			pixel.g.lower(6_b) & de,
			pixel.r.lower(6_b) & de
		);
		
		if (out.size() == 4)
			lvds.upper(7_b) = cat('0', pixel.b.upper(2_b), pixel.g.upper(2_b), pixel.r.upper(2_b));

		for(size_t i = 0; i < out.size(); ++i)
			out[i] = ~swapEndian(lvds.part(out.size(), i), 1_b);

		HCL_NAMED(out);
		return out;
	}

	void displayOpenLDIGateMate(const Clock& clkFast, std::deque<BVec> channel, std::string prefix)
	{
		Area ent{ "scl_displayOpenLDIGateMate", true };
		const Clock& clkSlow = ClockScope::getClk();

		Bit syncPulse = scl::synchronizeEvent('1', clkSlow, clkFast) | name("syncPulse");

		ClockScope ioScope{ clkFast };
		for (size_t i = 0; i < channel.size(); ++i)
		{
			std::string pinName = prefix + std::to_string(i);

			scl::arch::colognechip::CC_LVDS_OBUF buf;
			buf.pin(pinName + "p", pinName + "n");
			buf.regOut(true);
			buf.O() = scl::arch::colognechip::serdes(channel[i], clkSlow, clkFast, syncPulse);
		}

		std::string pinName = prefix + "C";

		scl::arch::colognechip::CC_LVDS_OBUF buf;
		buf.pin(pinName + "p", pinName + "n");
		buf.regOut(true);
		buf.O() = scl::arch::colognechip::serdes(~BVec("7b1100011"), clkSlow, clkFast, syncPulse);
	}

	std::list<std::deque<BVec>> displayControllerDualOpenLDI(TileLinkU&& dma, MemoryMap& mmap, size_t w, size_t h)
	{
		Area ent{ "scl_displayControllerDualOpenLDI", true };
		mmap.enterScope("display");

		// 1. fetch frame buffer over tile link
		UInt frameBufferOffset = dma.cfg.addrW;
		Bit enabled;
		mapIn(mmap, enabled, "enable");
		mapIn(mmap, frameBufferOffset, "framebuffer");

		Stream fetchCmd = strm::createVStream(TileLinkStreamFetch::Command{
			.address = frameBufferOffset,
			.beats = w * h / 2 / dma.cfg.dataW.bytes(),
		}, '1') | strm::attach(Ready{});

		Bit pixelBufferAlmostFull;
		RvStream<BVec> rawFetchData{32_b};

		dma <<= TileLinkStreamFetch{}
			.enableBursts(64 * 8)
			.pause(pixelBufferAlmostFull | !enabled)
			.generate(fetchCmd, rawFetchData, 1_b);

		Fifo<BVec> rawDataBuffer(w, rawFetchData->width());
		move(rawFetchData) | strm::push(rawDataBuffer);
		pixelBufferAlmostFull = rawDataBuffer.almostFull(w - 64) | name("pixelBufferAlmostFull");

		// 2. add display synchronization data
		scl::DisplayMode mode = {
			.pixelFreq = ClockScope::getClk().absoluteFrequency(),
			.w = {
				.resolution = w / 2, .syncStart = w / 2, .syncEnd = 0, .total = w * 5 / 4 / 2,
			},
			.h = {
				.resolution = h, .syncStart = h, .syncEnd = 0, .total = h + 20,
			},
		};
		VStream<DisplaySync, BVec> sync = displaySync(mode)
			| strm::removeReady(flag(pixelBufferAlmostFull, '0')) // wait for first wave of pixel data
			| strm::regDownstream()
			| displaySynchronizeData(strm::pop(rawDataBuffer) | name("rawPixelWord") | strm::widthReduce(8_b) | name("rawPixel"))
			| name("sync");
		rawDataBuffer.generate();

		// 3. convert to RGB
		BitWidth colorDepth = 6_b;
		Memory<ColorRGB> colorTable(4_b, ColorRGB{ .r = colorDepth, .g = colorDepth, .b = colorDepth });
		colorTable.noConflicts();
		colorTable.fillPowerOnState(
			displayColorTableDefaultValue(4_b, colorDepth, colorDepth, colorDepth)
		);
		colorTable.setType(MemType::DONT_CARE, 2);
		mapIn(mmap, colorTable, "color");
		
		const BVec& raw = get<BVec>(sync);
#if 0
		size_t syncDelay = 2;
		ColorRGB odd = colorTable[(UInt)raw.lower(4_b)].read() | reg({.allowRetimingBackward = true}) | reg({ .allowRetimingBackward = true }) | name("odd");
		ColorRGB even = colorTable[(UInt)raw.upper(4_b)].read() | reg({.allowRetimingBackward = true}) | reg({ .allowRetimingBackward = true }) | name("even");
#else
		size_t syncDelay = 0;
		UInt oddVal = cat(raw.lower(4_b), "2b00");
		ColorRGB odd{ oddVal, oddVal, oddVal };
		UInt evenVal = cat(raw.upper(4_b), "2b00");
		ColorRGB even{ evenVal, evenVal, evenVal };
#endif

		std::list<std::deque<BVec>> out = {
			displayOpenLDIEncode(odd, scl::delay(sync->onScreen, syncDelay), '0', '0'),
			displayOpenLDIEncode(even, scl::delay(sync->onScreen, syncDelay), '0', '0'),
		};
		HCL_NAMED(out);

		// backlight control
		UInt backlightLevel = 8_b;
		mapIn(mmap, backlightLevel, "backlight_level");
		BacklightCtrl bl = backlightController(backlightLevel);
		pinOut(bl.powerEn, "DISPLAY_EN_12V");
		pinOut(!bl.ledEn, "DISPLAY_BL_EN");		// TODO: remove inversion
		pinOut(!bl.ledPwm, "DISPLAY_BL_PWM");

		mmap.leaveScope();
		return out;
	}

	DisplayControllerTiled::DisplayControllerTiled() :
		m_colorTable(packColorTable())
	{
	}

	DisplayControllerTiled& DisplayControllerTiled::tileFormat(size_t w, size_t h, size_t bpp)
	{
		HCL_DESIGNCHECK(w % 2 == 0);
		m_tileW = w;
		m_tileH = h;
		m_tileBpp = bpp;
		return *this;
	}

	DisplayControllerTiled& DisplayControllerTiled::tileData(sim::DefaultBitVectorState data)
	{
		m_tiledata = move(data);
		return *this;
	}
	
	DisplayControllerTiled& DisplayControllerTiled::resolution(size_t w, size_t h, size_t syncw, size_t synch)
	{
		m_w = w;
		m_h = h;
		m_syncW = syncw;
		m_syncH = synch;
		return *this;
	}

	DisplayControllerTiled& DisplayControllerTiled::colorTable(sim::DefaultBitVectorState data)
	{
		m_colorTable = move(data);
		return *this;
	}

	RvStream<std::array<std::deque<BVec>, 2>> DisplayControllerTiled::generate(TileLinkU&& dma, MemoryMap& mmap)
	{
		Area ent{ "scl_DisplayControllerTiled", true };
		mmap.enterScope("display");

		Stream out =
			generateTileFetch(move(dma), mmap) |
			strm::attach(generateTileSync()) |
			name("tile") |
			std::bind(&DisplayControllerTiled::generateColorLookup, this, strm::_1) |
			name("color") |
			std::bind(&DisplayControllerTiled::generateEncode, this, strm::_1) |
			name("displaySignal")
			;

		generateBacklightControl(mmap);
		mmap.leaveScope();
		return out;
	}

	std::vector<std::array<double, 3>> DisplayControllerTiled::ansiColorTable()
	{
		std::vector<std::array<double, 3>> table = {
			{ .0, .0, .0 },
			{ .5, .0, .0 },
			{ .0, .5, .0 },
			{ .5, .5, .0 },
			{ .0, .0, .5 },
			{ .5, .0, .5 },
			{ .0, .5, .5 },
			{ .75, .75, .75 },
			{ .5, .5, .5 },
			{ 1., .0, .0 },
			{ .0, 1., .0 },
			{ 1., 1., .0 },
			{ .0, .0, 1. },
			{ 1., .0, 1. },
			{ .0, 1., 1. },
			{ 1., 1., 1. },
		};

		for (size_t r = 0; r < 6; ++r)
			for (size_t g = 0; g < 6; ++g)
				for (size_t b = 0; b < 6; ++b)
					table.push_back({ r / 5., g / 5., b / 5. });

		for (size_t gray = 0; gray < 24; ++gray)
		{
			double level = 0.03125 + gray / 23. * 0.8984375;
			table.push_back({ level, level, level });
		}

		HCL_ASSERT(table.size() == 256);
		return table;
	}

	sim::DefaultBitVectorState DisplayControllerTiled::packColorTable(ColorTableCfg cfg)
	{
		sim::DefaultBitVectorState table;
		table.resize(cfg.colors.size() * cfg.colorDepth * 3);

		for (size_t index = 0; index < cfg.colors.size(); ++index)
		{
			std::array<double, 3>& rgb = cfg.colors[index];

			size_t val = 0;
			for (size_t i = 0; i < rgb.size(); ++i)
			{
				// gamma correction
				double c = pow(rgb[i], cfg.gammaCorrection);
				// quantization
				size_t q = (size_t)round(c * ((1 << cfg.colorDepth) - 1));
				// pack
				val |= q << i * cfg.colorDepth;
			}

			table.insert(sim::DefaultConfig::VALUE, index * cfg.colorDepth * 3, cfg.colorDepth * 3, val);
		}

		table.setRange(sim::DefaultConfig::DEFINED, 0, table.size());
		return table;
	}

	static std::deque<Bit> counterCascade(std::deque<size_t> end)
	{
		Area ent{ "scl_counterCascade", true };

		std::deque<Bit> out;
		
		Bit precondition = '1';
		for (size_t i = 0; i < end.size(); ++i)
		{
			Counter ctr(end[i]);
			IF(precondition)
				ctr.inc();
			precondition &= ctr.isLast();
			out.push_back(precondition);
		}
		HCL_NAMED(out);
		return out;
	}

	RvStream<DisplayControllerTiled::Tile> DisplayControllerTiled::generateTileFetch(TileLinkU&& dma, MemoryMap& mmap)
	{
		Area ent{ "scl_generateTileFetch", true };
		HCL_DESIGNCHECK((m_w + m_syncW) % m_tileW == 0);
		HCL_DESIGNCHECK((m_h + m_syncH) % m_tileH == 0);

		UInt frameBufferOffset = dma.cfg.addrW;
		Bit enabled;
		mapIn(mmap, enabled, "enable");
		mapIn(mmap, frameBufferOffset, "framebuffer");

		const size_t numTilesX = (m_w + m_syncW) / m_tileW;
		const size_t numTilesY = (m_h + m_syncH) / m_tileH;
		const size_t tileSize = width(Tile{}).bytes();
		const size_t beatSize = dma.cfg.dataW.bytes();
		const size_t burstSize = std::min(utils::lowestSetBitMask(numTilesX * numTilesY * tileSize), 64ull);
		HCL_DESIGNCHECK(burstSize >= beatSize);

		// fetch from memory
		Stream fetchCmd = strm::createVStream(TileLinkStreamFetch::Command{
			.address = frameBufferOffset,
			.beats = (numTilesX * numTilesY * tileSize) / beatSize,
			}, '1') | strm::attach(Ready{});

		Bit pauseFetch;
		RvStream<BVec> rawFetchData{ dma.cfg.dataW };

		const BitWidth sourceW = 1_b;
		dma <<= TileLinkStreamFetch{}
			.enableBursts(burstSize / beatSize * 8)
			.pause(pauseFetch | !enabled)
			.generate(fetchCmd, rawFetchData, sourceW);

		const size_t fifoSize = numTilesX * (tileSize / beatSize) * 2; // capture at least two lines of data
		TransactionalFifo<BVec> rawDataBuffer(fifoSize, rawFetchData->width());
		move(rawFetchData) | strm::push(rawDataBuffer);
		pauseFetch = rawDataBuffer.almostFull((sourceW.count() + 1) * burstSize / beatSize) | name("pauseFetch");

		// repeat buffered data for each line of current tile row
		RvStream<BVec> bufferOut = strm::pop(rawDataBuffer);
		IF(transfer(bufferOut))
		{
			const auto& ctr = counterCascade({ numTilesX * tileSize / beatSize, m_tileH });
			IF(ctr[0])
				rawDataBuffer.rollbackPop();
			IF(ctr[1])
				rawDataBuffer.commitPop();
		}

		Bit fullLineFetched = flag(!rawDataBuffer.almostEmpty(numTilesX * tileSize / beatSize), rawDataBuffer.empty()) | name("fullLineFetched");

		// gather full Tile data
		RvStream<Tile> out = move(bufferOut)
			| name("fifo_out")
			| strm::widthExtend(width(Tile{}))
			| name("extended")
			| strm::transform([](const BVec& data) { Tile t; unpack(data, t); return t; })
			| strm::stall(!fullLineFetched)
			| name("unpacked")
			| strm::regDownstream()
			| name("out");

		rawDataBuffer.generate();

		//ready(out) = '0';
		//return strm::createVStream(Tile{ .color = {"8x9", "8xA", "8xB", "8xD"}, .index = ConstUInt('X', 32_b) }, '1') | strm::attach(Ready{});

		return out;
	}

	RvStream<DisplayControllerTiled::TileSync> DisplayControllerTiled::generateTileSync()
	{
		const size_t numTilesX = (m_w + m_syncW) / m_tileW;
		const size_t numLines = m_h + m_syncH;

		RvStream<TileSync> sync;
		valid(sync) = '1';
		ENIF(transfer(sync))
		{
			const auto& ctr = counterCascade({ numTilesX, numLines });
			sync->eol = ctr[0];
			sync->eof = ctr[1];
		}

		HCL_NAMED(sync);
		return sync;
	}

	void DisplayControllerTiled::generateBacklightControl(MemoryMap& mmap)
	{
		UInt backlightLevel = 8_b;
		mapIn(mmap, backlightLevel, "backlight_level");
		BacklightCtrl bl = backlightController(backlightLevel);
		pinOut(bl.powerEn, "DISPLAY_EN_12V");
		pinOut(bl.ledEn, "DISPLAY_BL_EN");
		pinOut(bl.ledPwm, "DISPLAY_BL_PWM");
	}

	RvStream<std::array<std::deque<BVec>, 2>> DisplayControllerTiled::generateEncode(RvStream<std::array<ColorRGB, 2>>&& color)
	{
		Area ent{ "scl_generateEncode", true };

		auto generateDataEnable = [this]() {
			scl::Counter y(m_h + m_syncH);
			scl::Counter x((m_w + m_syncW) / 2);

			IF(x.isLast())
				y.inc();

			return (x.value() < m_w / 2) & (y.value() < m_h) | name("de");
		};

		auto encode = [](const std::array<ColorRGB, 2>& color, const Bit& de) {
			return std::array<std::deque<BVec>, 2>{{
				displayOpenLDIEncode(color[0], de, '0', '0'),
				displayOpenLDIEncode(color[1], de, '0', '0'),
			}};
		};

		return move(color) | name("color") |
			strm::transform<strm::InsertPos::attach>(generateDataEnable) |
			strm::transform(encode) |
			name("out") |
			strm::remove<Bit>();
	}

	size_t DisplayControllerTiled::numTiles() const
	{
		return m_tiledata.size() / (m_tileH * m_tileW * m_tileBpp);
	}

	RvStream<std::array<ColorRGB, 2>> DisplayControllerTiled::generateColorLookup(RvStream<Tile, TileSync>&& tile)
	{
		Area ent{ "scl_generateColorLookup", true };

		constexpr size_t pixelPerCycle = 2;

		Memory<BVec> tileData(m_tiledata.size() / (m_tileBpp * 2), BitWidth(m_tileBpp * 2));
		tileData.setName("tileData");
		tileData.fillPowerOnState(m_tiledata);

		BitWidth colorIndexW = tile->color.begin()->width();
		BitWidth colorDepth(m_colorTable.size() / colorIndexW.count() / 3);
		Memory<ColorRGB> colorTable(colorIndexW, { .r = colorDepth, .g = colorDepth, .b = colorDepth });
		colorTable.setName("colorTable");
		colorTable.fillPowerOnState(m_colorTable);

		auto calcTileDataAddress = [this](const Tile& tile, const TileSync& sync, const Eop& eop) {
			Counter ctrX{ m_tileW / pixelPerCycle };
			Counter ctrY{ m_tileH };

			IF(sync.eol & eop.eop)
			{
				ctrX.reset();
				ctrY.inc();
			}
			IF(sync.eof & eop.eop)
				ctrY.reset();

			// calculate tile data memory address from in tile pixel position and tile index
			return cat(ctrY.value(), tile.index.lower(BitWidth::count(numTiles())), ctrX.value());
		};

		auto lookupColor = [&](const BVec& tileData, const Tile& tile) {
			std::array<ColorRGB, pixelPerCycle> ret;
			for (size_t i = 0; i < ret.size(); ++i)
				ret[i] = colorTable[mux(tileData.part(pixelPerCycle, i), tile.color)];
			return ret;
		};

		return
			strm::duplicate(move(tile), m_tileW / pixelPerCycle) |
			strm::transform<strm::InsertPos::payload>(calcTileDataAddress) |
			name("tiledataaddr") |
			strm::lookup(tileData) |
			name("tiledata") |
			strm::transform(lookupColor) |
			strm::delay(colorTable.readLatencyHint(), { .allowRetimingBackward = true }) |
			name("out") |
			strm::reduceTo<RvStream<std::array<ColorRGB, pixelPerCycle>>>()
			;
	}
}
