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


// Apparently no longer needed, but keeping around in case it is needed for webassembly clang
/*
#ifdef __clang__
#include <experimental/coroutine>

namespace std {
	using namespace experimental;
}

#else
*/

//#include <coroutine>

/*
#endif
*/


#ifdef __clang__
#define __cpp_impl_coroutine true
#define __cpp_lib_coroutine true
#endif

#include <coroutine>

/*
#ifdef __clang__
namespace std::experimental {
	using std::coroutine_traits;
	using std::coroutine_handle;
	using std::suspend_always;
	using std::suspend_never;
}
#endif
 */

