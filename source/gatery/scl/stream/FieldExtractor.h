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
#include "Stream.h"
#include "Packet.h"
#include "../Counter.h"

#include <boost/format.hpp>

namespace gtry::scl
{
	/// Specification of a field to extract from a packet stream
	struct Field {
		/// Bit-offset of the field into the stream
		size_t offset;
		/// Size (int bits) of the field to extract
		BitWidth size;
	};

	/**
	 * @brief Extracts fields of fixed sizes from fixed offsets of a packet stream.
	 * @details The fields may be misaligned wrt. beat boundaries and may even span two or more beats.
	 * Once all fields have been captured (but potentially while the packets is still being streamed in), they
	 * are presented at the output stream.
	 * @param output A valid stream of the extracted fields. If this stream has backpressure (i.e. a ready field), the packetStream must also support backpressure.
	 * @param packetStream The input packet stream to extract the fields from.
	 */
	template<Signal... MetaOutput, Signal... MetaInput> 
		requires (
			(!Stream<gtry::Vector<BVec>, MetaOutput...>::template has<Ready>() || Stream<BVec, MetaInput...>::template has<Ready>()) && // Backpressure on output requires backpressure on input
			Stream<gtry::Vector<BVec>, MetaOutput...>::template has<Valid>() && // The output stream is invalid until the header has been fully extracted
			Stream<gtry::Vector<BVec>, MetaOutput...>::template has<Error>() && // The output stream must report on packets being too short
			Stream<BVec, MetaInput...>::template has<Eop>() // The input must be a packet stream
		)
	inline void extractFields(Stream<gtry::Vector<BVec>, MetaOutput...> &output, Stream<BVec, MetaInput...> &packetStream, const std::span<Field> fields)
	{
		Area area("fieldExtractor", true);

		BitWidth beatWidth = packetStream->width();

		// Figure out the last beat to be able to size the beat counter correctly
		size_t lastHeaderBeat = 0;
		for (const auto &field : fields) {
			if (field.size == 0_b) continue;

			size_t lastBeat = (field.offset + field.size.value-1) / beatWidth.value;
			lastHeaderBeat = std::max(lastHeaderBeat, lastBeat);
		}
		// Figure out how much of the last beat is needed in case we have an Empty field
		size_t requiredBitsInLastHeaderBeat = 0;
		for (const auto &field : fields) {
			if (field.size == 0_b) continue;

			size_t lastBeat = (field.offset + field.size.value-1) / beatWidth.value;
			size_t lastBit = (field.offset + field.size.value-1) % beatWidth.value;
			if (lastBeat == lastHeaderBeat)
				requiredBitsInLastHeaderBeat = std::max(requiredBitsInLastHeaderBeat, lastBit+1);
		}


		// Create a counter that counts the beats and is used
		// to determine when to extract fields from the stream
		scl::Counter beatCount(lastHeaderBeat+2);

		Reg<Bit> fieldsExtracted('0');
		fieldsExtracted.setName("fieldsExtracted");
		Reg<Bit> outputValid('0');
		outputValid.setName("outputValid");
		Reg<Bit> outputTransfered('0');
		outputTransfered.setName("outputTransfered");
		Reg<Bit> packetFullyIngested('0');
		packetFullyIngested.setName("packetFullyIngested");
		Reg<UInt> txidStore;
		if constexpr (packetStream.template has<TxId>()) {
			txidStore.constructFrom(txid(packetStream));
			txidStore.setName("txidStore");
		}

		// We can receive data if either:
		// - We have not yet fully ingested to current packet (i.e. for discarding the payload)
		// - The output has been transfered and we are ready for the next packet
		if constexpr (packetStream.template has<Ready>())
			ready(packetStream) = !packetFullyIngested | outputTransfered;

		IF (scl::transfer(packetStream)) {
			// Have the counter stick once all fields have been extracted.
			// Otherwise, count the beats
			IF (!fieldsExtracted)
				beatCount.inc();

			// If in this cycle we hit the last beat that needs to be
			// sampled, all fields will be extracted and the output will
			// be ready.
			IF (beatCount.value() == lastHeaderBeat) {
				outputValid = '1';

				// If there is no Empty field, then at this point we have everything to fully
				// extract all fields. If there is an empty field, and if this is the EOP (where Empty is valid), 
				// then we need to check if this beat actually contains sufficient bytes for all fields.
				if constexpr (packetStream.template has<Empty>()) {
					size_t requiredBytes = (requiredBitsInLastHeaderBeat + 7) / 8;
					size_t maxEmptyBytes = packetStream->size() / 8 - requiredBytes;
					fieldsExtracted = !scl::eop(packetStream) | (scl::empty(packetStream) <= maxEmptyBytes);
				} else
					fieldsExtracted = '1';
			}

			// If we hit the eop, remember that we fully ingested the packet
			// and can continue if or once the output has been transfered.
			IF (scl::eop(packetStream)) {
				packetFullyIngested = '1';

				// If we have not extracted the header by now, mark the output as valid
				// nontheless and set the error flag.
				IF (!fieldsExtracted.combinatorial())
					outputValid = '1';
			}

			// Since we may transmit the output after the packet has been fully ingested, we may have to remember the txid
			if constexpr (packetStream.template has<TxId>())
				txidStore = txid(packetStream);
		}

		// Potentially forward additional fields
		if constexpr (output.template has<TxId>() && packetStream.template has<TxId>())
			txid(output) = txidStore.combinatorial();


		// If we ran out of packets before the last header field, we become valid without having extracted everything.
		// In this case, set the error flag.
		error(output) = outputValid.combinatorial() & !fieldsExtracted.combinatorial();
		valid(output) = outputValid.combinatorial();

		// If we transfer the output, it is no longer valid
		// but mark that we transferred and may proceed with
		// the next packet if or once the current one has been
		// fully ingested.
		IF (transfer(output)) {
			outputTransfered = '1';
			outputValid = '0';
		}

		// If (in this cycle) both, the output was or is being transferred and
		// the packet was or is fully ingested, then reset for the next packet.
		IF (outputTransfered.combinatorial() & packetFullyIngested.combinatorial()) {
			beatCount.reset();
			outputTransfered = '0';
			packetFullyIngested = '0';
			fieldsExtracted = '0';
		}		

		output->resize(fields.size());
		// Build extractors for all fields
		for (auto fieldIdx : utils::Range(fields.size())) {
			const auto &field = fields[fieldIdx];
			if (field.size == 0_b) continue;

			// All outputs are registered to hold their values until
			// all fields have been gathered and then until they have
			// been transferred.

			BVec fieldStore = field.size;
			setName(fieldStore, (boost::format("fieldStore_%d") % fieldIdx).str());

			output->at(fieldIdx) = fieldStore;
			fieldStore = reg(fieldStore);

			IF (valid(packetStream)) {
				// Bit address of the end of the field
				size_t fieldEnd = field.offset + field.size.value;
				// The beat in which this field starts
				size_t firstBeat = field.offset / beatWidth.value;
				// The beat in which this field ends
				size_t lastBeat = (fieldEnd-1) / beatWidth.value;

				// The field may be fully contained within one beat
				// but it may also span the border between two beats
				// or even multiple beats
				for (size_t beat = firstBeat; beat <= lastBeat; beat++) {
					bool isFirstBeat = beat == firstBeat;
					bool isLastBeat = beat == lastBeat;

					// The bit offset of the start of this beat within the packet
					size_t beatStart = beat * beatWidth.value;

					// Wait for the beat to come around and if so, grab the (partial) field
					// from the stream.
					IF (beatCount.value() == beat) {

						// Since the field and the stream may be arbitrarily misaligned,
						// it is entirely possible that in this beat only part of the 
						// field "field-slice" is extracted.

						// The bit offset wrt. the beat where the field-slice starts.
						size_t streamIntraBeatOffset = isFirstBeat ? field.offset - beatStart : 0;
						// The bit offset wrt. the field where the field-slice starts.
						size_t intraFieldOffset = isFirstBeat ? 0 : beatStart - field.offset;
						// The size (in bits) of the field-slice.
						BitWidth sliceWidth = isLastBeat ? BitWidth(fieldEnd - beatStart-streamIntraBeatOffset) : beatWidth - streamIntraBeatOffset;

						// Extract and store in output. The output payload is registered and will hold this value until
						// it is overwritten for the next packet.
						fieldStore(intraFieldOffset, sliceWidth) = (*packetStream)(streamIntraBeatOffset, sliceWidth);
					}
				}
			}
		}
	}



	/**
	 * @brief Extracts a monolithic header from a packet stream.
	 */
	template<Signal... MetaOutput, Signal... MetaInput, Signal Header> 
		requires (
			Stream<Header, MetaOutput...>::template has<Ready>() == Stream<BVec, MetaInput...>::template has<Ready>() &&
			Stream<Header, MetaOutput...>::template has<Valid>() &&
			Stream<BVec, MetaInput...>::template has<Eop>())
	inline void extractHeader(Stream<Header, MetaOutput...> &output, Stream<BVec, MetaInput...> &packetStream, size_t offset = 0)
	{
		std::array<Field, 1> fields = {
			Field {
				.offset = offset,
				.size = width(*output),
			},
		};

		scl::Stream<Vector<BVec>, MetaOutput...> fieldStream;
		extractFields(fieldStream, packetStream, fields);

		output <<= fieldStream.transform([&output](const Vector<BVec> &fields) {
			Header header = constructFrom(*output);
			unpack(fields[0], header);
			return header;
		});
	}

}