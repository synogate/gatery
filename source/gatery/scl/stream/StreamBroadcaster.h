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
#include <ranges>

#include "Stream.h"
#include "../utils/OneHot.h"
#include "../views.h"
#include "../Counter.h"
#include "CreditStream.h" //temporary

#include <memory>

namespace gtry::scl
{

	/**
	 * @brief Broadcasts a stream to multiple sinks, making sure that all sinks receive all data.
	 * @details If the sinks have backpressure, the source is correctly backpressured so that no sink
	 * misses a transmission.
	 */
	template<StreamSignal StreamT>
	class StreamBroadcaster
	{
		public:
			StreamBroadcaster(StreamT &&stream) : StreamBroadcaster(stream) { }

			StreamBroadcaster(StreamT &stream) { 
				m_helper = std::make_shared<Helper>();
				m_helper->source = constructFrom(stream);
				m_helper->source <<= stream;
				if constexpr (StreamT::template has<Ready>())
					ready(m_helper->source) = '1';
				if constexpr (StreamT::template has<scl::strm::Credit>()) {
					auto& sourceCredit = m_helper->source.template get<scl::strm::Credit>();
					*sourceCredit.increment = '1';
				}
			}

			void broadcastTo(StreamT &sink) {
				downstream(sink) = downstream(m_helper->source);

				if constexpr (StreamT::template has<Ready>()) {
					ready(m_helper->source) &= ready(sink);

					Bit othersReady = '1';
					for (auto &s : m_helper->sinks) {
						s.valid &= ready(sink);
						othersReady &= s.ready;
					}

					m_helper->sinks.push_back({});
					auto &sinkCtrl = m_helper->sinks.back();

					valid(sink) = sinkCtrl.valid;
					sinkCtrl.valid = valid(m_helper->source) & othersReady;
					sinkCtrl.ready = ready(sink);
				}

				if constexpr (StreamT::template has<scl::strm::Credit>()) {

					UInt counter = BitWidth::last(m_helper->source.template get<scl::strm::Credit>().maxCredit);
					counter = reg(counter, 0);

					auto& sourceCredit = m_helper->source.template get<scl::strm::Credit>();
					IF(counter == 0)
						*sourceCredit.increment = '0';
					
					Bit increment = *sink.template get<scl::strm::Credit>().increment;
					Bit decrement = final(*sourceCredit.increment);
					
					UInt change = ConstUInt(0, counter.width());
					IF(decrement & !increment)
						change |= '1'; //effectively makes change = -1
					
					IF(increment & !decrement)
						change = 1;
					counter += change;
				}
			}

			operator StreamT() {
				StreamT res;
				broadcastTo(res);
				return res;
			}

			StreamT bcastTo() {
				StreamT res;
				broadcastTo(res);
				return res;
			}
		protected:
			struct Helper {
				StreamT source;

				struct SinkControl {
					Bit valid;
					Bit ready;
				};
				std::list<SinkControl> sinks;
			};
			/// Decouple everything important into a shared heap object so that the broadcaster can be copied
			std::shared_ptr<Helper> m_helper;
	};

	/// Overload of connect(...) for StreamBroadcaster to allow using the <<= operator with broadcasters.
	using gtry::connect;

	template<StreamSignal StreamT>
	void connect(StreamT& sink, StreamBroadcaster<StreamT>& broadcaster) { broadcaster.broadcastTo(sink); }

}
