/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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

#include <gatery/frontend/ExternalComponent.h>
#include <gatery/frontend/tech/TechnologyMappingPattern.h>


namespace gtry::scl::arch::xilinx {

class IOBUF : public gtry::ExternalComponent
{
	public:

		enum Inputs {
			IN_I,
			IN_T,
        	IN_IO_I,
			IN_COUNT
		};
		enum Outputs {
        	OUT_O,
        	OUT_IO_O,

			OUT_COUNT
		};

		enum DriveStrength {
			DS_2mA,
			DS_4mA,
			DS_6mA,
			DS_8mA,
			DS_12mA,
			DS_16mA,
			DS_24mA,
		};

		IOBUF();

		void setDriveStrength(DriveStrength driveStrength);

		virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;
};

}
