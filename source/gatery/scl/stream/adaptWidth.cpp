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
#include "adaptWidth.h"

using StreamA = gtry::scl::RvStream<gtry::UInt>;
using StreamB = gtry::scl::RvStream<gtry::UInt, gtry::scl::ByteEnable>;

template auto gtry::scl::extendWidth<StreamA>(StreamA&&, gtry::BitWidth, gtry::Bit);
template auto gtry::scl::extendWidth<StreamB>(StreamB&&, gtry::BitWidth, gtry::Bit);

#if 0
gtry::scl::Stream<gtry::UInt> gtry::scl::adaptWidth(Stream<UInt>& source, BitWidth width, Bit reset)
{
	auto scope = Area{ "scl_adaptWidth" }.enter();
	Stream<UInt> ret;

	if(width > source->width())
	{
		const size_t ratio = width / source->width();

		UInt counter = BitWidth::count(ratio);
		IF(transfer(source))
		{
			IF(counter == ratio - 1)
				counter = 0;
			ELSE
				counter += 1;
		}
		IF(reset)
			counter = 0;

		Bit counter_last = reg(counter == ratio - 1, '0');
		HCL_NAMED(counter_last);
		counter = reg(counter, 0);
		HCL_NAMED(counter);

		ret.valid = counter_last & source.valid;
		ret.data = width;
		ret.data = reg(ret.data);

		IF(transfer(source))
		{
			ret.data >>= (int)source->width().bits();
			ret.data.upper(source->width()) = source.data;
		}

		*source.ready = *ret.ready | !counter_last;
	}
	else if(width == source.data.width())
	{
		ret <<= source;
	}
	else
	{
		const size_t ratio = source->width() / width;

		UInt counter = BitWidth::count(ratio);
		IF(*ret.ready)
		{
			IF(counter == ratio - 1)
				counter = 0;
			ELSE
				counter += 1;
		}
		IF(!source.valid | reset)
			counter = 0;

		Bit counter_last = reg(counter == ratio - 1, '0');
		HCL_NAMED(counter_last);
		counter = reg(counter, 0);
		HCL_NAMED(counter);

		*source.ready = '0';
		IF(ready(ret) & counter_last)
			*source.ready = '1';

		ret.valid = source.valid;
		ret.data = source.data(zext(counter, width.bits()) * width.bits(), width);
	}
	HCL_NAMED(ret);
	return ret;
}

gtry::scl::Stream<gtry::scl::Packet<gtry::UInt>> gtry::scl::adaptWidth(Stream<gtry::scl::Packet<gtry::UInt>>& source, BitWidth width)
{
	auto scope = Area{ "scl_adaptWidth" }.enter();
	Stream<Packet<UInt>> ret;

	if(width > source->width())
	{
		const size_t ratio = width / source->width();

		UInt counter = BitWidth::count(ratio);
		IF(transfer(source))
		{
			IF(counter == ratio - 1)
				counter = 0;
			ELSE
				counter += 1;
		}
		IF(eop(source))
			counter = 0;

		Bit counter_last = reg(counter == ratio - 1, '0');
		HCL_NAMED(counter_last);
		counter = reg(counter, 0);
		HCL_NAMED(counter);

		ret.valid = counter_last & source.valid;
		*ret.data = width;
		ret.data.eop = source.data.eop;
		ret.data = reg(ret.data);

		IF(transfer(source))
		{
			*ret.data >>= (int)source->width().bits();
			ret.data->upper(source->width()) = *source.data;
		}

		*source.ready = *ret.ready | !counter_last;
	}
	else if(width == source->width())
	{
		ret <<= source;
	}
	else
	{
		const size_t ratio = source->width() / width;

		UInt counter = BitWidth::count(ratio);
		IF(*ret.ready)
		{
			IF(counter == ratio - 1)
				counter = 0;
			ELSE
				counter += 1;
		}
		IF(!source.valid | (source.data.eop & transfer(source)))
			counter = 0;

		Bit counter_last = reg(counter == ratio - 1, '0');
		HCL_NAMED(counter_last);
		counter = reg(counter, 0);
		HCL_NAMED(counter);

		*source.ready = '0';
		IF(ready(ret) & counter_last)
			* source.ready = '1';

		ret.valid = source.valid;
		*ret.data = source.data.data(zext(counter, width.bits()) * width.bits(), width);
		ret.data.eop = source.data.eop & counter_last;
	}
	HCL_NAMED(ret);
	return ret;
}
#endif
