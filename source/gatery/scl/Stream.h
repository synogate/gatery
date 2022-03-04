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
#include <optional>

namespace gtry::scl
{
    template<typename Payload>
    struct Stream
    {
        Payload data;

        std::optional<Bit> valid;
        std::optional<Bit> ready;
        std::optional<Bit> sop;
        std::optional<Bit> eop;
        std::optional<Bit> error;

        Bit transfer() const;
    };

    template<typename Payload>
    struct Valid
    {
        Valid() = default;

        template<typename... PayloadArgs>
        Valid(const Bit& validValue, PayloadArgs... ctorArgs) :
            data(ctorArgs...),
            valid(validValue) 
        {}

        BOOST_HANA_DEFINE_STRUCT(Valid,
            (Bit, valid),
            (Payload, data)
        );

        Payload& value() { return data; }
        const Payload& value() const { return data; }
    };

    template<typename Payload>
    struct StreamSource;

    template<typename Payload>
    struct StreamSink : Payload
    {
        using Payload::Payload;
        StreamSink(StreamSource<Payload>& source);
        StreamSink(const StreamSink&) = delete;
        StreamSink(StreamSink&&) = default;

        Bit valid;// = true;
        Bit ready;// = true;
    };

    template<typename Payload>
    struct StreamSource : Payload
    {
        using Payload::Payload;
        StreamSource(const StreamSource&) = delete;
        StreamSource(StreamSource&&) = default;

        Bit valid;// = true;
        Bit ready;// = true;

        void operator >> (StreamSink<Payload>& sink);
    };

    template<typename Payload>
    void connect(StreamSource<Payload>& source, StreamSink<Payload>& sink)
    {
        (Payload&)sink = (Payload&)source; ///@todo wire in order independent fashion
        source.ready = sink.ready;
        sink.valid = source.valid;
    }

    template<typename Payload>
    inline void StreamSource<Payload>::operator>>(StreamSink<Payload>& sink)
    {
        connect(*this, sink);
    }
    
    template<typename Payload>
    inline StreamSink<Payload>::StreamSink(StreamSource<Payload>& source)
    {
        connect(source, *this);
    }
    
    template<typename Payload>
    inline Bit Stream<Payload>::transfer() const
    {
        Bit transaction = '1';
        if (valid)
            transaction &= *valid;
        if (ready)
            transaction &= *ready;
        HCL_NAMED(transaction);
        return transaction;
    }
}
