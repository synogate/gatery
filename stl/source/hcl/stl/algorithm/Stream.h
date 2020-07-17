#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
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
        //primordial((Payload&)source) <= (Payload&)sink;
        (Payload&)sink = (Payload&)source; ///@todo wire in order independent fashion
        primordial(source.ready) <= sink.ready;
        primordial(sink.valid) <= source.valid;
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
