#pragma once
#include <hcl/frontend.h>
#include <optional>

namespace hcl::stl
{
    template<typename Payload>
    struct Stream : Payload
    {
        template<typename... PayloadArgs>
        Stream(PayloadArgs... ctorArgs) :
            Payload(ctorArgs...)
        {}

        std::optional<Bit> valid;
        std::optional<Bit> ready;
        std::optional<Bit> sop;
        std::optional<Bit> eop;
        std::optional<Bit> error;

        Payload& value() { return *this; }
        const Payload& value() const { return *this; }
    };

    template<typename Payload>
    struct Valid : Payload
    {
        Valid() = default;

        template<typename... PayloadArgs>
        Valid(const core::frontend::Bit& validValue, PayloadArgs... ctorArgs) :
            Payload(ctorArgs...),
            valid(validValue) 
        {}

        core::frontend::Bit valid = '1';

        Payload& value() { return *this; }
        const Payload& value() const { return *this; }
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

        core::frontend::Bit valid;// = true;
        core::frontend::Bit ready;// = true;
    };

    template<typename Payload>
    struct StreamSource : Payload
    {
        using Payload::Payload;
        StreamSource(const StreamSource&) = delete;
        StreamSource(StreamSource&&) = default;

        core::frontend::Bit valid;// = true;
        core::frontend::Bit ready;// = true;

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
}
