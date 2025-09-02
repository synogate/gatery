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
#pragma once
#include <gatery/frontend.h>
#include "../memoryMap/MemoryMap.h"
#include "../stream/strm.h"
#include "../tilelink/tilelink.h"
#include "DisplaySync.h"

namespace gtry::scl
{
	struct ColorRGB
	{
		UInt r, g, b;
	};


	RvStream<ColorRGB> displayColorTable(RvStream<UInt> index, MemoryMap& ctrl, const ColorRGB& blueprint);


	/**
	 * @brief Combines pixel data and display synchronization signals into a packet stream for display output.
	 *			Pixel data is resynchronized to the display timing during blank periods if pixel data is not available in time.
	 */
	template<Signal Pixel>
	VStream<DisplaySync, Pixel> displaySynchronizeData(VStream<DisplaySync> sync, RvStream<Pixel> pixel);

	template<Signal Pixel>
	inline auto displaySynchronizeData(RvStream<Pixel>&& pixel)
	{
		return [&pixel](auto&& in) { return displaySynchronizeData(std::forward<decltype(in)>(in), move(pixel)); };
	}

	/**
	 * @brief Create gamma corrected identity color lookup table, used as default initialization value for color tables.
	 */
	sim::DefaultBitVectorState displayColorTableDefaultValue(BitWidth indexW, BitWidth rW, BitWidth gW, BitWidth bW);

	/**
	 * @brief Enocde RGB pixel data into Open LVDS Display Interface signals. 
	 *			On dual pixel interfaces the even pixel channel should set de, vsync, hsync to '0'.
	 *			Most if not all displays ignore vsync and hsync by synchronizing to de period length.
	 * 
	 * @param pixel 6b or 8b RGB pixel value.
	 * @param de Data enable signal to signal on screen pixel.
	 * @param vsync Optional vertical sync pulse.
	 * @param hsync Optional horizontal sync pulse.
	 * @return Array of 7b LVDS channels.
	 */
	std::deque<BVec> displayOpenLDIEncode(ColorRGB pixel, Bit de, Bit vsync, Bit hsync);

	void displayOpenLDIGateMate(const Clock& clkFast, std::deque<BVec> channel, std::string prefix = "DISPLAY_RXO");


	std::list<std::deque<BVec>> displayControllerDualOpenLDI(TileLinkU&& dma, MemoryMap& mmap, size_t w, size_t h);

	class DisplayControllerTiled
	{
	public:
		struct Tile
		{
			std::array<UInt, 4> color = { 8_b, 8_b, 8_b, 8_b };
			UInt index = 32_b;
		};

		struct TileSync
		{
			Bit eol, eof; // end of line and end of frame
		};

		// generates an ANSI color table. see https://en.wikipedia.org/wiki/ANSI_escape_code for details.
		static std::vector<std::array<double, 3>> ansiColorTable();

		struct ColorTableCfg
		{
			size_t colorDepth = 6;
			double gammaCorrection = 2.2;
			std::vector<std::array<double, 3>> colors = ansiColorTable();
		};

		static sim::DefaultBitVectorState packColorTable(ColorTableCfg cfg = {});
	public:
		DisplayControllerTiled();

		DisplayControllerTiled& tileFormat(size_t w, size_t h, size_t bpp);

		DisplayControllerTiled& tileData(sim::DefaultBitVectorState data);
		DisplayControllerTiled& resolution(size_t w, size_t h, size_t syncw, size_t synch);
		DisplayControllerTiled& colorTable(sim::DefaultBitVectorState data);

		virtual RvStream<std::array<std::deque<BVec>, 2>> generate(TileLinkU&& dma, MemoryMap& mmap);

	protected:
		virtual RvStream<std::array<ColorRGB, 2>> generateColorLookup(RvStream<Tile, TileSync>&& tile);
		virtual RvStream<Tile> generateTileFetch(TileLinkU&& dma, MemoryMap& mmap);
		virtual RvStream<TileSync> generateTileSync();
		virtual void generateBacklightControl(MemoryMap& mmap);

		// default implementation is dual pixel OpenLDI encoding
		virtual RvStream<std::array<std::deque<BVec>, 2>> generateEncode(RvStream<std::array<ColorRGB, 2>>&& color);

		size_t numTiles() const;

	private:
		size_t m_w = 0;
		size_t m_h = 0;
		size_t m_syncW = 0;
		size_t m_syncH = 0;
		size_t m_tileW = 0;
		size_t m_tileH = 0;
		size_t m_tileBpp = 0;
		sim::DefaultBitVectorState m_tiledata;
		sim::DefaultBitVectorState m_colorTable;
	};

}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::ColorRGB, r, g, b);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::DisplayControllerTiled::Tile, color, index);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::DisplayControllerTiled::TileSync, eol, eof);

namespace gtry::scl
{
	template<Signal Pixel>
	VStream<DisplaySync, Pixel> displaySynchronizeData(VStream<DisplaySync> sync, RvStream<Pixel> pixel)
	{
		Area ent{ "scl_displaySynchronizeData", true };
		HCL_NAMED(pixel);
		HCL_NAMED(sync);

		Counter missCtr(sync->x.width() + 4_b);

		IF(valid(sync) & sync->onScreen & !valid(pixel))
			missCtr.inc();
		IF(transfer(pixel) & !sync->onScreen)
			missCtr.dec();

		ready(pixel) = valid(sync) & sync->onScreen;
		IF(!missCtr.isFirst())
			ready(pixel) = '1'; // skip pixel until we are back in sync

		return attach(sync, *pixel) | name("out");
	}
}
