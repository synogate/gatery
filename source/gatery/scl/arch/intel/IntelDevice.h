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

#include "../general/FPGADevice.h"

namespace gtry::scl::arch::intel {

class IntelDevice : public FPGADevice {
	public:
		void fromConfig(const gtry::utils::ConfigTree &configTree) override;

		std::string nextLpmInstanceName(std::string_view macroType);

		void setupAgilex();
		void setupArria10();
		void setupStratix10();
		void setupCyclone10();
		void setupMAX10();

		void setupDevice(std::string device);

		/// Whether or not the device family (Arria 10 and Cyclone 10 GX) requires the "derive_pll_clocks" tcl instruction in their timing constraints file.
		bool requiresDerivePllClocks() const { return m_requiresDerivePllClocks; }
	protected:
		void setupCustomComposition(const gtry::utils::ConfigTree &customComposition);

		/// Whether or not the device family (Arria 10 and Cyclone 10 GX) requires the "derive_pll_clocks" tcl instruction in their timing constraints file.
		bool m_requiresDerivePllClocks = false;
		std::map<std::string_view, size_t> m_lpmInstanceCounter;
};

}

namespace gtry::scl {
	using IntelDevice = gtry::scl::arch::intel::IntelDevice;
}
