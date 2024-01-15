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


#include <vector>
#include <optional>
#include <variant>
#include <filesystem>
#include <span>

#include "../../simulation/BitVectorState.h"
#include <boost/iostreams/device/mapped_file.hpp>

namespace gtry::hlim {


/**
 * @brief Abstract interface for classes that hold the storage of memory in simulations.
 * @details These classes only handle the storage itself, not input and output delays, and also
 * not any form of collisions.
 * They do however handle undefined inputs correctly or at least pessimistic.
 */
class MemoryStorage {
	public:

		/**
		 * @brief How to initialize the memory
		 * @details Initialization is described in two "layers".
		 * 1. An optional background layer of regular data which, within its extends, is fully defined and 
		 * 2. an overlay of zero or more bit vectors, mapped to given bit-addresses, which may be partially undefined and override the background layer.
		 * In case of using the std::span<uint8_t> variant of the background data, the underlying data is not necessarily copied and must be kept around
		 * while the storage is in use.
		 */
		struct Initialization {
			/// Whether to fill (parts of) the memory with custom data or a memory mapped file.
			std::optional<std::variant<std::span<uint8_t>, std::filesystem::path>> background;
			/// Whether to initially overwrite that background with chunks of partially defined data at specific bit-addresses.
			std::vector<std::pair<std::uint64_t, sim::DefaultBitVectorState>> initialOverlay;

			static Initialization setAllDefinedRandom(size_t size, std::uint64_t offset = 0, uint32_t seed = 20231201);
		};

		virtual ~MemoryStorage() = default;

		/**
		 * @brief Read from memory
		 * @details Reads can be unaligned to any bit address and of any bit width, but must not span beyond the size of the memory.
		 * @param offset Location *in bits* to start reading from.
		 * @param size Size *in bits* to read
		 */
		sim::DefaultBitVectorState read(std::uint64_t offset, std::uint64_t size) const {
			sim::DefaultBitVectorState result;
			read(result, offset, size);
			return result;
		}

		/**
		 * @brief Read from memory
		 * @details Reads can be unaligned to any bit address and of any bit width, but must not span beyond the size of the memory.
		 * @param offset Location *in bits* to start reading from.
		 * @param size Size *in bits* to read
		 */
		virtual void read(sim::DefaultBitVectorState &dst, std::uint64_t offset, std::uint64_t size) const = 0;

		/**
		 * @brief Write to memory
		 * @details Writes can be unaligned to any bit address and of any bit width, but must not span beyond the size of the memory.
		 * @param offset Location *in bits* to start reading from.
		 * @param value The bit vector to write to memory, which also defines the length of the write.
		 * @param undefinedWriteEnable If true, the write enable was not asserted but undefined and an undefined write is performed, potentially setting the targeted memory region to undefined.
		 * @param mask Either empty, or a bitwise mask of the write. An asserted bit indicates the write is to be made. Undefined bits are handled like undefinedWriteEnable.
		 */
		virtual void write(std::uint64_t offset, const sim::DefaultBitVectorState &value, bool undefinedWriteEnable, const sim::DefaultBitVectorState &mask) = 0;

		/// Returns the size of the memory in bits
		virtual std::uint64_t size() const = 0;

		/// Sets the entire memory to undefined, e.g. in case of a write to an undefined address.
		virtual void setAllUndefined() = 0;
};

/**
 * @brief Dense memory, storing everything into one big array.
 */
class MemoryStorageDense : public MemoryStorage
{
	public:	
		MemoryStorageDense(std::uint64_t size, const Initialization& initialization = {});

		using MemoryStorage::read;
		virtual void read(sim::DefaultBitVectorState &dst, std::uint64_t offset, std::uint64_t size) const override;
		virtual void write(std::uint64_t offset, const sim::DefaultBitVectorState &value, bool undefinedWriteEnable, const sim::DefaultBitVectorState &mask) override;

		virtual std::uint64_t size() const override { return m_memory.size(); }

		virtual void setAllUndefined() override;
	protected:
		sim::DefaultBitVectorState m_memory;
};

/**
 * @brief Sparse memory implementation, keeping the background isolated and tracking updates as overlayed sparse changes.
 * @details Changes are stored as sparse, non-overlapping chunks.
 * Writes overlapping the boundaries of previous writes will enlarge and potentially fuse these chunks.
 */
class MemoryStorageSparse : public MemoryStorage
{
	public:
		MemoryStorageSparse(std::uint64_t size, const Initialization& initialization = {});

		using MemoryStorage::read;
		virtual void read(sim::DefaultBitVectorState &dst, std::uint64_t offset, std::uint64_t size) const override;
		virtual void write(std::uint64_t offset, const sim::DefaultBitVectorState &value, bool undefinedWriteEnable, const sim::DefaultBitVectorState &mask) override;

		virtual std::uint64_t size() const override { return m_size; }
		virtual void setAllUndefined() override;
	protected:
		std::uint64_t m_size;
		std::span<const uint8_t> m_background;
		boost::iostreams::mapped_file_source m_mappedBackgroundFile;

		using OverlayMap = std::map<std::uint64_t, sim::DefaultBitVectorState>;
		OverlayMap m_overlay;

		template<typename Functor>
		void forEachOverlapping(std::uint64_t offset, std::uint64_t size, Functor functor) const;

		template<typename Functor>
		void forEachOverlapping(std::uint64_t offset, std::uint64_t size, Functor functor) {
			std::as_const(*this).forEachOverlapping(offset, size, [this, functor](OverlayMap::const_iterator it) {
				// Doesn't erase anything, but converts a const_iterator into a non-const iterator
				auto nonConstIt = m_overlay.erase(it, it);
				functor(nonConstIt);
			});
		}		

		void populateFromBackground(std::uint64_t offset, sim::DefaultBitVectorState &value, std::uint64_t valueOffset = 0, std::uint64_t valueSize = ~0ull) const;
};


}
