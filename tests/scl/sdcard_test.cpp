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
#include "scl/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/scl/memory/StorageDeviceModel.h>
#include <gatery/scl/memory/SdCardModel.h>

using namespace gtry;
using namespace gtry::scl;

BOOST_AUTO_TEST_CASE(storagemodel_vhdx_test)
{
	std::unique_ptr empty = std::make_unique<StorageDeviceModelEmpty>();
	std::unique_ptr vhdx = std::make_unique<StorageDeviceModelVHDX>(move(empty), "data/fat32_test.vhdx");

	std::span mbr = vhdx->sectorRead(0);
	BOOST_TEST(mbr.size() == 512);
	BOOST_TEST((mbr[510] == std::byte(0x55)));
	BOOST_TEST((mbr[511] == std::byte(0xAA)));

	std::span notPresent = vhdx->sectorRead(1337);
	BOOST_TEST(std::ranges::all_of(notPresent, [](std::byte b) { return b == std::byte(0); }));
}

BOOST_AUTO_TEST_CASE(storagemodel_memsparse_test)
{
	std::unique_ptr empty = std::make_unique<StorageDeviceModelEmpty>();
	std::unique_ptr vhdx = std::make_unique<StorageDeviceModelVHDX>(move(empty), "data/fat32_test.vhdx");
	std::unique_ptr sparsemem = std::make_unique<StorageDeviceModelMemSparse>(move(vhdx));

	// red from fallback vhdx
	std::span mbr = sparsemem->sectorRead(0);
	BOOST_TEST(mbr.size() == 512);
	BOOST_TEST((mbr[510] == std::byte(0x55)));
	BOOST_TEST((mbr[511] == std::byte(0xAA)));

	// read non existing sector
	std::span notPresent = sparsemem->sectorRead(1337);
	BOOST_TEST(std::ranges::all_of(notPresent, [](std::byte b) { return b == std::byte(0); }));

	// write, read, erase, read existing and non existing sector
	std::vector testdata(512, 'X');
	for (uint64_t s : {0, 1337})
	{
		sparsemem->sectorWrite(s, as_bytes(std::span(testdata)));

		std::span full = sparsemem->sectorRead(s);
		BOOST_TEST(std::ranges::all_of(full, [](std::byte b) { return b == std::byte('X'); }));

		sparsemem->sectorErase(s);

		std::span erased = sparsemem->sectorRead(s);
		BOOST_TEST(std::ranges::all_of(erased, [](std::byte b) { return b == std::byte(0); }));
	}
}

BOOST_AUTO_TEST_CASE(sdcardmodel_init_test)
{
	std::unique_ptr empty = std::make_unique<StorageDeviceModelEmpty>();

	SdCardModel sdcard;
	sdcard.storage(move(empty));

	// CMD0 GO_IDLE_STATE (reset to spi mode)
	for (uint8_t c : {0x40, 0x00, 0x00, 0x00, 0x00, 0x95})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 1);

	// CMD8 SEND_IF_COND
	for (uint8_t c : {0x48, 0x00, 0x00, 0x01, 0xAA, 0x87})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 1);
	BOOST_TEST(sdcard.data(0xFF) == 0);
	BOOST_TEST(sdcard.data(0xFF) == 0);
	BOOST_TEST(sdcard.data(0xFF) == 0x01);
	BOOST_TEST(sdcard.data(0xFF) == 0xAA);
	sdcard.data(0xFF);

	// CMD55 APP_CMD
	for (uint8_t c : {0x77, 0x00, 0x00, 0x00, 0x00, 0x00})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 1);

	// ACMD41 SD_SEND_OP_COND
	for (uint8_t c : {0x69, 0x40, 0x00, 0x00, 0x00, 0x00})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 0);
}

BOOST_AUTO_TEST_CASE(sdcardmodel_read_test)
{
	std::unique_ptr mem = std::make_unique<StorageDeviceModelMemSparse>(nullptr);

	std::vector<std::uint8_t> testdata(512, 0xAA);
	mem->sectorWrite(5, as_bytes(std::span(testdata)));

	SdCardModel sdcard;
	sdcard.storage(move(mem));

	for (uint8_t c : {0x51, 0x00, 0x00, 0x00, 0x05, 0x00})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 0);

	BOOST_TEST(sdcard.data(0xFF) == 0xFE);
	for (size_t i = 0; i < 512; ++i)
		BOOST_TEST((sdcard.data(0xFF) == testdata[i]));
}

BOOST_AUTO_TEST_CASE(sdcardmodel_csd_test)
{
	std::unique_ptr vhdx = std::make_unique<StorageDeviceModelVHDX>(nullptr, "data/fat32_test.vhdx");
	uint64_t numSectorsVHDX = vhdx->numSectors();

	SdCardModel sdcard;
	sdcard.storage(move(vhdx));

	for (uint8_t c : {64 + 9, 0x00, 0x00, 0x00, 0x00, 0x00})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 0);

	BOOST_TEST(sdcard.data(0xFF) == 0xFE);

	for(size_t i = 0; i < 6; ++i)
		sdcard.data(0xFF); // skip not implemented bits

	uint32_t C_SIZE = 0;
	C_SIZE |= sdcard.data(0xFF) << 24ul;
	C_SIZE |= sdcard.data(0xFF) << 16ul;
	C_SIZE |= sdcard.data(0xFF) << 8ul;
	C_SIZE |= sdcard.data(0xFF);

	uint64_t numSectorsSD = (C_SIZE + 1) * 1024;
	BOOST_TEST(numSectorsVHDX == numSectorsSD);
}

BOOST_AUTO_TEST_CASE(sdcardmodel_write_test)
{
	std::unique_ptr mem = std::make_unique<StorageDeviceModelMemSparse>(nullptr);

	SdCardModel sdcard;
	sdcard.storage(move(mem));

	for (uint8_t c : {64 + 24, 0x00, 0x00, 0x00, 0x05, 0x00})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 0);

	BOOST_TEST(sdcard.data(0xFE) == 0xFF);
	for (size_t i = 0; i < 512; ++i)
		BOOST_TEST(sdcard.data(0xAA) == 0xFF);
	BOOST_TEST(sdcard.data(0) == 0xFF);
	BOOST_TEST(sdcard.data(0) == 0xFF);

	BOOST_TEST(sdcard.data(0xFF) == 5); // write accepted
	BOOST_TEST(sdcard.data(0xFF) == 0); // write busy
}

BOOST_AUTO_TEST_CASE(sdcardmodel_status_test)
{
	std::unique_ptr mem = std::make_unique<StorageDeviceModelMemSparse>(nullptr);

	SdCardModel sdcard;
	sdcard.storage(move(mem));

	for (uint8_t c : {64 + 13, 0x00, 0x00, 0x00, 0x00, 0x00})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 0);
	BOOST_TEST(sdcard.data(0xFF) == 0);

	sdcard.reset();

	for (uint8_t c : {64 + 13, 0x00, 0x00, 0x00, 0x00, 0x00})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 1);
	BOOST_TEST(sdcard.data(0xFF) == 0);
}

BOOST_AUTO_TEST_CASE(sdcardmodel_erase_test)
{
	std::unique_ptr mem = std::make_unique<StorageDeviceModelMemSparse>(nullptr);

	SdCardModel sdcard;
	sdcard.storage(move(mem));

	for (uint8_t c : {64 + 32, 0x00, 0x00, 0x00, 0x10, 0x00})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 0);

	for (uint8_t c : {64 + 33, 0x00, 0x00, 0x10, 0x00, 0x00})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 0);

	for (uint8_t c : {64 + 38, 0x00, 0x00, 0x00, 0x00, 0x00})
		BOOST_TEST(sdcard.data(c) == 0xFF);
	BOOST_TEST(sdcard.data(0xFF) == 0);
}
