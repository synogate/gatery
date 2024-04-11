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

#include <span>
#include <ostream>

/**
 * @addtogroup gtry_frontend_logging
 * @{
 */

namespace gtry::dbg {

class BMP {
    public:
        BMP(size_t width, size_t height, size_t bpp);

        void setPixels(std::span<const unsigned char> data, size_t stride = 0);
        void setPalette(std::span<const uint32_t> data, size_t stride = 0);

        void writeBinary(std::ostream &stream);
        void writeBase64Binary(std::ostream &stream);
    protected:
        size_t m_width = 0;
        size_t m_height = 0;
        size_t m_bpp = 0;

        std::span<const unsigned char> m_pixelData;
        size_t m_pixelStride;

        std::span<const uint32_t> m_paletteData;
        size_t m_paletteStride;


};

}

/**@}*/