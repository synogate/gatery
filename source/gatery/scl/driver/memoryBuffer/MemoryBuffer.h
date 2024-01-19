/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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

/*
 * Do not include the regular gatery headers since this is meant to compile stand-alone in driver/userspace application code. 
 */

#include "../utils.h"

#include <memory>

#include <cstddef>
#include <span>
#include <vector>
#include <cstdint>
#include "../utils.h"

/**
 * @addtogroup gtry_scl_driver
 * @{
 */

namespace gtry::scl::driver {

	class MemoryBuffer;

	class LockedSpan {
		public:
			template<typename T> requires (std::is_trivially_copyable_v<T>)
			inline std::span<T> view() { return std::span<T>((T*)m_data.data(), m_data.size() / sizeof(T)); }

			LockedSpan(MemoryBuffer &buffer, std::span<std::byte> data);
			~LockedSpan();

			inline operator std::span<std::byte>() { return view<std::byte>(); }
		private:
			MemoryBuffer &m_buffer;
			std::span<std::byte> m_data;
	};

	class ConstLockedSpan {
		public:
			template<typename T> requires (std::is_trivially_copyable_v<T>)
			inline std::span<const T> view() { return std::span<const T>((const T*)m_data.data(), m_data.size() / sizeof(T)); }

			ConstLockedSpan(MemoryBuffer &buffer, std::span<const std::byte> data);
			~ConstLockedSpan();

			inline operator std::span<const std::byte>() { return view<std::byte>(); }
		private:
			MemoryBuffer &m_buffer;
			std::span<const std::byte> m_data;
	};

	class MemoryBuffer {
		public:
			enum class Flags : std::uint32_t {
				DISCARD	  = 1 << 0,
				READ_ONLY = 1 << 1,
			};
			
			MemoryBuffer(std::uint64_t size);
			virtual ~MemoryBuffer() = default;

			inline std::uint64_t size() const { return m_size; }
			inline bool canRead() const { return m_canRead; }
			inline bool canWrite() const { return m_canWrite; }

			inline LockedSpan map(Flags flags) { return LockedSpan(*this, lock(flags)); }
			inline ConstLockedSpan map(Flags flags) const { return ConstLockedSpan(const_cast<MemoryBuffer&>(*this), lock((Flags)((std::uint32_t)flags | (std::uint32_t)Flags::READ_ONLY))); }

			inline std::span<const std::byte> lock(Flags flags) const { return const_cast<MemoryBuffer*>(this)->lock((Flags)((std::uint32_t)flags | (std::uint32_t)Flags::READ_ONLY)); }
			virtual std::span<std::byte> lock(Flags flags) = 0;
			virtual void unlock() = 0;

			virtual void write(std::span<const std::byte> data) = 0;
			virtual void read(std::span<std::byte> data) const = 0;

			virtual void writeFromBuffer(const MemoryBuffer &other);
			virtual void readToBuffer(MemoryBuffer &other) const;
		protected:
			std::uint64_t m_size = 0;
			std::uint64_t m_accessAlignment = 1;
			bool m_canRead = true;
			bool m_canWrite = true;

			void checkFlags(Flags flags);
	};

	class MemoryBufferFactory {
		public:
			virtual ~MemoryBufferFactory() = default;

			virtual std::unique_ptr<MemoryBuffer> allocate(uint64_t bytes) = 0;
		protected:
			template<typename T>
			inline std::unique_ptr<T> allocateDerivedImpl(uint64_t bytes) {
				auto buf = allocate(bytes);
				return std::unique_ptr<T>(static_cast<T*>(buf.release()));
			}
	};

}

/**@}*/