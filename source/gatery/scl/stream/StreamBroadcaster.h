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

#include <memory>

namespace gtry::scl
{

	/**
	 * @brief Broadcasts a stream to multiple sinks, making sure that all sinks receive all data.
	 * @details If the sinks have backpressure, the source is correctly backpressured so that no sink
	 * misses a transmission.
	 */
	template<StreamSignal T> requires ( 
				T::template has<Ready>() == T::template has<Valid>())
	class StreamBroadcaster
	{
		public:
			StreamBroadcaster(T &&stream) : StreamBroadcaster(stream) { }

			StreamBroadcaster(T &stream) { 
				m_helper = std::make_shared<Helper>();
				m_helper->source = constructFrom(stream);
				m_helper->source <<= stream;
				ready(m_helper->source) = '1';
			}

			void broadcastTo(T &sink) {
				downstream(sink) = downstream(m_helper->source);

				if constexpr (T::template has<Ready>()) {
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
			}

			operator T() {
				T res;
				broadcastTo(res);
				return res;
			}

			T bcastTo() {
				T res;
				broadcastTo(res);
				return res;
			}
		protected:
			struct Helper {
				T source;

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
	template<StreamSignal T>
	void connect(T& sink, StreamBroadcaster<T>& broadcaster) { broadcaster.broadcastTo(sink); }

}
