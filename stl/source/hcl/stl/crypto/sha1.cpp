/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "sha1.h"
#include "sha2.h"
#include "md5.h"
#include "../Adder.h"

template struct hcl::stl::Sha1Generator<>;
template struct hcl::stl::Sha0Generator<>;

template struct hcl::stl::Sha2_256<>;

template struct hcl::stl::Md5Generator<>;

template class hcl::stl::HashEngine<hcl::stl::Sha1Generator<>>;
