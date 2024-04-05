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
#include "gatery/pch.h"

#include "BMP.h"

#include "../../utils/Range.h"
#include "../../utils/Exceptions.h"
#include "../../utils/Preprocessor.h"


#include <sstream>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

namespace gtry::dbg {

BMP::BMP(size_t width, size_t height, size_t bpp)
{
    m_width = width;
    m_height = height;
    m_bpp = bpp;

    HCL_DESIGNCHECK_HINT(
        (m_bpp == 1) ||
        (m_bpp == 4) ||
        (m_bpp == 8) ||
        (m_bpp == 16) ||
        (m_bpp == 24) ||
        (m_bpp == 32), "Invalid bpp");    
}

void BMP::setPixels(std::span<const unsigned char> data, size_t stride)
{
    m_pixelData = data;
    if (stride > 0)
        m_pixelStride = stride;
    else {
        HCL_DESIGNCHECK_HINT(m_width * m_bpp % 8 == 0, "Pitch (row stride of pixel data) must be a multiple of one byte");
        m_pixelStride = (m_width * m_bpp)/8;
    }
}

void BMP::setPalette(std::span<const uint32_t> data, size_t stride)
{
    m_paletteData = data;
    if (stride > 0)
        m_paletteStride = stride;
    else
        m_paletteStride = 1;
}

GTRY_PACKED(BMPFileHeader);
GTRY_PACKED(BitmapInfoHeader);

struct BMPFileHeader
{
    char magic[2] = {'B', 'M'};
    uint32_t fileSize;
    uint16_t reserved[2] = {0, 0 };
    uint32_t bitmapDataOffset;
};

struct BitmapInfoHeader
{
    uint32_t headerSize = 40;
    uint32_t bitmapWidth;
    uint32_t bitmapHeight;
    uint16_t numColorPlanes = 1;
    uint16_t bpp;
    uint32_t compressionMethod = 0; // BI_RGB == none
    uint32_t imageSize = 0; // can be 0 for compression BI_RGB
    uint32_t resolutionHor = 1000;
    uint32_t resolutionVer = 1000;
    uint32_t paletteColors = 0; // may be zero for 2^bpp colors
    uint32_t numImportantColors = 0;
};


void BMP::writeBinary(std::ostream &stream)
{
    BMPFileHeader header;
    BitmapInfoHeader infoHeader;

    size_t rowSize = (m_width * m_bpp + 7)/8;
    size_t paddedRowSize = (rowSize + 3) / 4 * 4;
    size_t pixelSize = m_height * paddedRowSize;
    size_t paletteSize = m_paletteData.size() / m_paletteStride * 4;

    header.bitmapDataOffset = (uint32_t)( sizeof(BMPFileHeader) + sizeof(BitmapInfoHeader) + paletteSize );
    header.fileSize = (uint32_t)( header.bitmapDataOffset + pixelSize );
    infoHeader.bitmapWidth = (uint32_t) m_width;
    infoHeader.bitmapHeight = (uint32_t) (- (int)m_height);
    infoHeader.bpp = (uint16_t) m_bpp;
    if (m_bpp <= 8)
        infoHeader.paletteColors = (uint32_t) ( m_paletteData.size() / m_paletteStride );


    stream.write((const char *) &header, sizeof(header));
    stream.write((const char *) &infoHeader, sizeof(infoHeader));
    for (auto i : utils::Range(infoHeader.paletteColors)) {
        uint32_t color = m_paletteData[i*m_paletteStride] & 0x00FFFFFF;
        stream.write((const char *) &color, sizeof(color));
    }

    size_t rowPaddingBytes = paddedRowSize - rowSize;
    for (auto y : utils::Range(m_height)) {
        stream.write((const char *) &m_pixelData[y * m_pixelStride], rowSize);
        for ([[maybe_unused]] auto i : utils::Range(rowPaddingBytes))
            stream.put(0x00);
    }
}

void BMP::writeBase64Binary(std::ostream &stream)
{
    std::stringstream buffer;
    writeBinary(buffer);

    using namespace boost::archive::iterators;
    typedef 
        base64_from_binary<
           transform_width<
                std::string::iterator,
                6,
                8
            >
        > 
        base64_text;

    auto str = buffer.str();

    std::copy(
        base64_text(str.begin()),
        base64_text(str.end()),
        ostream_iterator<char>(stream)
    );
}

}