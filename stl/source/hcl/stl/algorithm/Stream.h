#pragma once
#include <hcl/frontend.h>
#include <optional>

namespace hcl::stl
{
#if 0
	class StreamSource;
	class StreamSink;

	class Stream {
		public:
			using Bit = core::frontend::Bit;
			using BVec = core::frontend::BVec;

			StreamSource getSource();
			StreamSink getSink();
		protected:
			BVec m_data;
			Bit m_valid;
			Bit m_ready;
			unsigned m_readyAllowance;
			unsigned m_readyLatency;

			friend class StreamSource;
			friend class StreamSink;
	};


	class StreamSource {
		public:
			using Bit = core::frontend::Bit;
			using BVec = core::frontend::BVec;

			void ignoreReadyAllowance(bool ignore) { m_ignoreReadyAllowance = ignore; }

			Bit full() { 
				Bit delayed = false;
				if (m_ignoreReadyAllowance && m_stream.m_readyAllowance > 0) {
					if (counter < ConstBVec(m_stream.m_readyAllowance))
						delayed = true;
				}
				return delay(!*m_stream.m_ready, m_stream.m_readyLatency) | delayed; 
			}
			void push(BVec data) { 
				m_stream.m_valid = true; m_stream.m_data = data; 
				if (m_ignoreReadyAllowance && m_stream.m_readyAllowance > 0) {
					IF (delay(*m_stream.m_ready, m_stream.m_readyLatency))
						counter = 0;
					ELSE
						counter += 1;
				}
			}
		protected:
			Stream &m_stream;			
			Register<BVec> counter;
			bool m_ignoreReadyAllowance = true;
	};

	/*


	//connect_bypass(output, input);
	StreamSource output{input};
	//input.ready = output.ready;
	//output.valid = input.valid;
	output.data = input.data + 1;
	//output.readyLatency = input.readyLatency;


	IF (!output.full()) {
		IF (input.pop(data))
			output.push(data + 1);
	}


	*/


	/*

	output.ready(input.ready());

	IF (!output.full()) {
		IF (input1.pop(data))
			output.push(data);
		ELSE
			IF (input2.pop(data))
				output.push(data);
	}

	*/



	class StreamSink {
		public:
			using Bit = core::frontend::Bit;
			using BVec = core::frontend::BVec;

			Bit pop(BVec &data) { m_stream.m_ready = true; data = *m_stream.m_data; return *m_stream.m_valid; }
		protected:
			Stream &m_stream;
	};











	Bit eth0_en = true;
	IF(*eth0_en)
		input1 = eth0_source(); // valid
	eth0_en = input1.ready();

	input2 = eth1_source(); // valid

	buffer1 = fifo(input1); // s&f packet
	buffer2 = fifo(input2); // s&f packet
	arbiter(buffer1, buffer2, arbiter_output); // ready

	rs232_sink(arbiter_output); // ready



	

	class FancyStreamSource
	{
		public:
			using Bit = core::frontend::Bit;
			using BVec = core::frontend::BVec;

			void valid(const Bit& value) { m_valid = value; }
			Bit &valid() { if (!m_valid) m_valid = false; return *m_valid; }

		protected:			
			BVec data;
			std::optional<Bit> m_valid;
			std::optional<Bit> m_ready;
			std::optional<Bit> m_sop;
			std::optional<Bit> m_eop;
			std::optional<unsigned> readyAllowance;
	};



	class FancyStreamSink
	{
		public:
			using Bit = core::frontend::Bit;
			using BVec = core::frontend::BVec;

			void ready(const Bit& value) { m_ready = value; }
			Bit &ready() { if (!m_ready) m_ready = false; return *m_ready; }

			const Bit& valid() { if(!m_valid) m_valid = true; return **m_valid; }
			const BVec& data() const { return m_data; }
		protected:			
			BVec m_data;
			std::optional<Bit> m_valid;
			std::optional<Bit> m_ready;
			std::optional<Bit> m_sop;
			std::optional<Bit> m_eop;
			std::optional<unsigned> readyAllowance;
	};
	
	
	template<typename Payload>
	void connect(FancyStreamSource<Payload> &source, FancyStreamSink<Payload> &sink) {

		if(sink.m_eop)
		{
			if(!source.m_eop)
			{
				sink.m_eop = true;
			}
		}
		if (sink.m_ready) {

		}

		*source.m_ready = **sink.m_ready;
		*sink.m_valid = **source.m_valid;
		sink.m_data = *source.data;
	}


#endif

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
        source.ready = *sink.ready;
        sink.valid = *source.valid;
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
