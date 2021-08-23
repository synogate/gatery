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
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/zip.hpp>

namespace gtry
{

    struct RegisterSettings {
        bool allowRetimingBackward = false;
        bool allowRetimingForward = false;
    };


	template<class T, class En = void>
	struct Reg
	{
		T operator () (const T& val, const RegisterSettings &settings = {}) { return val; }
	};

    struct RegTransform
    {
        RegTransform(const RegisterSettings &settings = {}) : settings(settings) { }

        template<typename T>
        T operator() (const T& val) { return Reg<T>{}(val, settings); }

        const RegisterSettings &settings;
    };
    
    template<typename ...T>
    struct Reg<std::tuple<T...>>
    {
        std::tuple<T...> operator () (const std::tuple<T...>& val, const RegisterSettings &settings = {}) { return boost::hana::transform(val, RegTransform{settings}); }
    };

	template<typename T>
	T reg(const T& val, const RegisterSettings &settings = {}) { return Reg<T>{}(val, settings); }

	template<typename T, typename Tr>
	T reg(const T& val, const Tr& resetVal, const RegisterSettings &settings = {}) { return Reg<T>{}(val, resetVal, settings); }
}
