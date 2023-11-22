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

#include <map>
#include <list>
#include <string_view>
#include <variant>
#include <iostream>

namespace gtry::scl
{
	struct AvalonMM
	{
		AvalonMM() = default;
		AvalonMM(AvalonMM&&) = default;
		AvalonMM(const AvalonMM&) = delete;

		AvalonMM& operator=(AvalonMM&&) = default;
		AvalonMM& operator=(const AvalonMM&) = delete;

		UInt address;
		std::optional<Bit> ready;
		std::optional<Bit> read;
		std::optional<Bit> write;
		std::optional<UInt> writeData;
		std::optional<UInt> readData;
		std::optional<Bit> readDataValid;
		std::optional<UInt> response;
		std::optional<Bit> writeResponseValid;
		std::optional<UInt> byteEnable;

		size_t readLatency = 0;
		size_t readyLatency = 0;
		size_t maximumPendingReadTransactions = 1;
		size_t maximumPendingWriteTransactions = 0;
		size_t minimumResponseLatency = 1;

		std::map<std::string_view, Selection> addressSel;
		std::map<std::string_view, Selection> dataSel;

		void pinIn(std::string_view prefix);
		void pinOut(std::string_view prefix);
		void setName(std::string_view prefix);
		
		template<typename T>
		void connect(Memory<T>& mem, BitWidth dataWidth = 32_b);

		void createReadDataValid();
		void createReadLatency(size_t targetLatency);

		enum class ResponseCode
		{
			OKAY = 0,
			RESERVED = 1,   // reserved address
			SLVERR = 2,	 // unsuccessful transfer
			DECODEERROR = 3 // undefined address
		};
	};

	Memory<UInt> attachMem(AvalonMM& avmm, BitWidth addrWidth = BitWidth{});

	class AvalonNetworkSection
	{
	public:
		AvalonNetworkSection(std::string name = "");

		void clear();
		void add(std::string name, AvalonMM port);
		AvalonNetworkSection& addSection(std::string name);

		AvalonMM& find(std::string_view path);

		void assignPins();
		AvalonMM demux();

	protected:
		std::string m_name;
		std::vector<std::pair<std::string, AvalonMM>> m_port;
		std::list<AvalonNetworkSection> m_subSections;
	};

	template<typename T>
	inline void AvalonMM::connect(Memory<T>& mem, BitWidth dataWidth)
	{
		struct SigInfo
		{
			std::string name;
			ElementarySignal* signalVec;
			Bit* signalBit = nullptr;
		
			Selection from;
		};
		
		struct SigVis : CompoundNameVisitor
		{
			virtual void elementaryOnly(ElementarySignal& a) final override
			{
				if (auto *bit = dynamic_cast<Bit*>(&a)) {
					handleBit(*bit);
				} else {
					for (size_t i = 0; i < a.size(); i += regWidthLimit)
					{
						SigInfo& sig = regMap.emplace_back().emplace_back();
						sig.name = makeName();
						sig.signalVec = &a;
						sig.from = Selection::Slice(i, std::min<size_t>(regWidthLimit, a.size() - i));
					}
				}
			}
		
			void handleBit(Bit& a)
			{
				if (regMap.empty() ||
					regMap.back().front().signalVec ||
					currentRegWidth == regWidthLimit)
				{
					regMap.emplace_back();
					currentRegWidth = 0;
				}
		 
				SigInfo& sig = regMap.back().emplace_back();
				sig.name = makeName();
				sig.signalBit = &a;
			}
		
			std::vector<std::vector<SigInfo>> regMap;
			size_t currentRegWidth = 0;
			size_t regWidthLimit = 1;
		};
		
		UInt memAddress = mem.addressWidth();
		HCL_NAMED(memAddress);
		
		auto&& port = mem[memAddress];
		T memContent = port.read();
		
		SigVis v;
		v.regWidthLimit = dataWidth.value;
		VisitCompound<T>{}(memContent, v);
		
		BitWidth regAddrWidth = BitWidth::count(v.regMap.size());
		address = regAddrWidth + mem.addressWidth();
		memAddress = address(regAddrWidth.value, mem.addressWidth());
		
		write = Bit{};
		writeData = dataWidth;
		readData = ConstUInt(0, dataWidth);
		UInt regAddress = address(0, regAddrWidth);
		HCL_NAMED(regAddress);
		for (size_t r = 0; r < v.regMap.size(); ++r)
		{
			IF(regAddress == r)
			{
				auto& reg = v.regMap[r];
				if (reg.size() == 1 && reg.front().signalVec)
				{
					SigInfo& sig = reg.front();
					UInt source = UInt(sig.signalVec->readPort())(sig.from);
					readData = zext(source);
		
					IF(*write) {
						source = (*writeData)(0, BitWidth{ (size_t)sig.from.width });
						sig.signalVec->assign(source.readPort());
					}
				}
				else
				{
					for (size_t i = 0; i < reg.size(); ++i)
					{
						readData.value()[i] = *reg[i].signalBit;
						(*reg[i].signalBit) = (*writeData)[i];
					}
				}
			}
		}

		*readData = reg(*readData, {.allowRetimingBackward=true});
		
		IF(*write)
			port = memContent;
		
	}
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::AvalonMM, address, ready, read, write, writeData, readData, readDataValid, readLatency, readyLatency, addressSel, dataSel);
