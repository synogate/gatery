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
#include "gatery/scl_pch.h"

#include <gatery/scl/Fifo.h>

#include <gatery/hlim/NodeGroup.h>

#include <gatery/frontend/DesignScope.h>
#include "FifoPattern.h"

#include "FIFO_SYNC_MACRO.h"

namespace gtry::scl::arch::xilinx {

bool FifoPattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
{
	// Only attempt to replace fifos
	if (nodeGroup->getName() != "scl_fifo") return false;

	const auto *meta = dynamic_cast<FifoMeta*>(nodeGroup->getMetaInfo());
	if (meta == nullptr) return false;
	const FifoCapabilities::Choice &fifoChoice = meta->fifoChoice;

	// Validate that we can handle the chosen fifo capabilities
	if (!fifoChoice.singleClock) return false;

	if (fifoChoice.latency_writeToEmpty != 1) return false;
	if (fifoChoice.latency_readToFull != 1) return false;
	if (fifoChoice.latency_writeToAlmostEmpty != 1) return false;
	if (fifoChoice.latency_readToAlmostFull != 1) return false;

	if (fifoChoice.readWidth != fifoChoice.writeWidth) return false;

	bool outputIsFallthrough = fifoChoice.latency_writeToEmpty == 0;

	NodeGroupSurgeryHelper groupHelper(nodeGroup);

	// As of now, only support one almost_full and one almost_empty signal, don't try to emulate, and skip entire instantiation if that is not possible
	if (meta->almostEmptySignalLevel.size() > 1) return false;
	size_t almostEmptyLevel = 0;
	if (!meta->almostEmptySignalLevel.empty()) {
		HCL_ASSERT(groupHelper.getAllSignals(meta->almostEmptySignalLevel.front().second).size() == 1);

		// Find threshold value
		auto *level = groupHelper.getAllSignals(meta->almostEmptySignalLevel.front().second).front();

		auto val = evaluateStatically({.node = level, .port = 0});
		if (val.size() > 64) return false;
		if (!sim::allDefined(val, 0, val.size())) return false;
		almostEmptyLevel = *val.data(sim::DefaultConfig::VALUE);
		if (utils::Log2C(almostEmptyLevel+1) > 13) return false;
	}

	if (meta->almostFullSignalLevel.size() > 1) return false;
	size_t almostFullLevel = 0;
	if (!meta->almostFullSignalLevel.empty()) {
		HCL_ASSERT(groupHelper.getAllSignals(meta->almostFullSignalLevel.front().second).size() == 1);

		// Find threshold value
		auto *level = groupHelper.getAllSignals(meta->almostFullSignalLevel.front().second).front();

		auto val = evaluateStatically({.node = level, .port = 0});

		if (val.size() > 64) return false;
		if (!sim::allDefined(val, 0, val.size())) return false;
		almostFullLevel = *val.data(sim::DefaultConfig::VALUE);
		HCL_ASSERT(fifoChoice.readWidth == fifoChoice.writeWidth);
		if (almostFullLevel > fifoChoice.readDepth) return false;
		almostFullLevel = fifoChoice.readDepth - almostFullLevel;
		if (utils::Log2C(almostFullLevel+1) > 13) return false;
	}


	// Find a clock (all must be the same, since fifoChoice.singleClock == true)
	hlim::Clock *clock = nullptr;
	for (auto &node : nodeGroup->getNodes()) {
		for (auto &c : node->getClocks())
			if (c != nullptr) {
				if (clock == nullptr)
					clock = c;
				else
					HCL_ASSERT(clock == c);
			}
	}

	HCL_ASSERT(clock != nullptr);


	// Decide on number and type of fifos (only stretch to cover width)
	// todo: use better algorithm to also cover depth
	size_t perFifoWidth_18k;
	size_t perFifoWidth_36k;
	switch (fifoChoice.readDepth) {
		case 512: 
			perFifoWidth_18k = 36; 
			perFifoWidth_36k = 72; 
		break;
		case 1024: 
			perFifoWidth_18k = 18; 
			perFifoWidth_36k = 36; 
		break;
		case 2048: 
			perFifoWidth_18k = 9; 
			perFifoWidth_36k = 18; 
		break;
		case 4096: 
			perFifoWidth_18k = 4; 
			perFifoWidth_36k = 9; 
		break;
		case 8192: 
			perFifoWidth_18k = 0; 
			perFifoWidth_36k = 4; 
		break;
		default:
			return false; // Abort for unsupported depths
	}


	// if we get to here, we are certain we can replace

	GroupScope scope(nodeGroup);
	/*
	Area xilinxImpl("xilinx_implementation");
	auto scope2 = xilinxImpl.enter();
	*/



	size_t num_18k = 0;
	size_t num_36k = 0;

	{
		num_36k = fifoChoice.readWidth / perFifoWidth_36k;
		size_t remaining = fifoChoice.readWidth % perFifoWidth_36k;
		if (remaining) {
			if (perFifoWidth_18k > 0 && remaining <= perFifoWidth_18k)
				num_18k++;
			else
				num_36k++;
		}
	}


	// Extract and hook important signals
	Bit in_valid = true;
	Bit out_ready = true;

	Bit empty;

	if (groupHelper.containsSignal("in_valid"))
		in_valid = groupHelper.getBit("in_valid");
	
	if (groupHelper.containsSignal("out_ready")) {

		if (outputIsFallthrough) {
			Clock frontendClock(clock);

			out_ready = groupHelper.getBit("out_ready");

			Bit first;
			first = frontendClock(first, '1');

			// Read from fifo (visible on next clock cycle) if not empty and either consumer is ready or it is the first word since reset
			out_ready = !empty & (first | out_ready);
			out_ready.setName("first_word_fallthrough_ready");
			IF (out_ready) first = false;
		} else {
			out_ready = groupHelper.getBit("out_ready");
		}
	}
	

	BVec in_data = groupHelper.getBVec("in_data_packed");
	BVec out_data = groupHelper.hookBVecBefore("out_data_packed");
	BVec out_data_accu = constructFrom(out_data);
	out_data_accu = 0;

	size_t width = in_data.width().value;
	HCL_ASSERT(width == fifoChoice.readWidth);


	// Construct fifos
	FIFO_SYNC_MACRO *lastFifo = nullptr;
	for (auto i : utils::Range(num_36k)) {
		auto *fifoMacro = DesignScope::createNode<FIFO_SYNC_MACRO>(perFifoWidth_36k, FIFO_SYNC_MACRO::FIFOSize::SIZE_36Kb);
		fifoMacro->setInput(FIFO_SYNC_MACRO::IN_WREN, in_valid);
		fifoMacro->setInput(FIFO_SYNC_MACRO::IN_RDEN, out_ready);
		fifoMacro->attachClock(clock, FIFO_SYNC_MACRO::CLK);

		size_t start = i * perFifoWidth_36k;
		size_t end = start + perFifoWidth_36k;
		end = std::min(end, width);

		BitWidth sectionWidth{ end - start };


		BVec inSection = BitWidth(perFifoWidth_36k);
		inSection = zext(in_data(start, sectionWidth));
		inSection.setName((boost::format("inSection_%d_%d") % start % end).str());
		fifoMacro->setInput(FIFO_SYNC_MACRO::IN_DI, inSection);

		BVec outSection = fifoMacro->getOutputBVec(FIFO_SYNC_MACRO::OUT_DO);
		out_data_accu(start, sectionWidth) = outSection(0, sectionWidth);

		out_data_accu.setName((boost::format("out_data_accu_0_%d") % end).str());

		lastFifo = fifoMacro;
	}

	for (auto i : utils::Range(num_18k)) {
		auto *fifoMacro = DesignScope::createNode<FIFO_SYNC_MACRO>(perFifoWidth_18k, FIFO_SYNC_MACRO::FIFOSize::SIZE_18Kb);
		fifoMacro->setInput(FIFO_SYNC_MACRO::IN_WREN, in_valid);
		fifoMacro->setInput(FIFO_SYNC_MACRO::IN_RDEN, out_ready);
		fifoMacro->attachClock(clock, FIFO_SYNC_MACRO::CLK);

		size_t start = num_36k*perFifoWidth_36k + i * perFifoWidth_18k;
		size_t end = start + perFifoWidth_18k;
		end = std::min(end, width);

		BitWidth sectionWidth{ end - start };

		BVec inSection = BitWidth(perFifoWidth_18k);
		inSection = zext(in_data(start, sectionWidth));
		inSection.setName((boost::format("inSection_%d_%d") % start % end).str());
		fifoMacro->setInput(FIFO_SYNC_MACRO::IN_DI, inSection);

		BVec outSection = fifoMacro->getOutputBVec(FIFO_SYNC_MACRO::OUT_DO);
		out_data_accu(start, sectionWidth) = outSection(0, sectionWidth);
		out_data_accu.setName((boost::format("out_data_accu_0_%d") % end).str());

		lastFifo = fifoMacro;
	}	

	// override data output
	out_data.exportOverride(out_data_accu);

	HCL_ASSERT(lastFifo != nullptr);

	// Attach full, empty, almost full and almost emppty signals to last fifo (they should all be equal).
	if (groupHelper.containsSignal("full")) {
		Bit full = groupHelper.hookBitBefore("full");
		full.exportOverride(lastFifo->getOutputBit(FIFO_SYNC_MACRO::OUT_FULL));
	}


	empty = lastFifo->getOutputBit(FIFO_SYNC_MACRO::OUT_EMPTY);
	HCL_NAMED(empty);
	if (groupHelper.containsSignal("empty")) {
		Bit e = groupHelper.hookBitBefore("empty");
		e.exportOverride(empty);
	}

	// As of now, only support one almost_full and one almost_empty signal and hook those to the last fifo
	if (!meta->almostEmptySignalLevel.empty()) {
		HCL_ASSERT(groupHelper.getAllSignals(meta->almostEmptySignalLevel.front().second).size() == 1);
	
		lastFifo->setAlmostEmpty(almostEmptyLevel);
		
		Bit almost_empty = groupHelper.hookBitBefore(meta->almostEmptySignalLevel.front().first);
		almost_empty.exportOverride(lastFifo->getOutputBit(FIFO_SYNC_MACRO::OUT_ALMOSTEMPTY));
	}

	if (!meta->almostFullSignalLevel.empty()) {
		HCL_ASSERT(groupHelper.getAllSignals(meta->almostFullSignalLevel.front().second).size() == 1);
	
		lastFifo->setAlmostFull(almostFullLevel);
		
		Bit almost_full = groupHelper.hookBitBefore(meta->almostFullSignalLevel.front().first);
		almost_full.exportOverride(lastFifo->getOutputBit(FIFO_SYNC_MACRO::OUT_ALMOSTFULL));
	}

	return true;
}


Xilinx7SeriesFifoCapabilities::~Xilinx7SeriesFifoCapabilities() 
{ 
}

Xilinx7SeriesFifoCapabilities::Choice Xilinx7SeriesFifoCapabilities::select(const Xilinx7SeriesFifoCapabilities::Request &request) const
{
	Choice choice;

	HCL_ASSERT_HINT(request.readWidth.choice == Preference::SPECIFIC_VALUE, "Read width must be specifc value!");
	HCL_ASSERT_HINT(request.writeWidth.choice == Preference::SPECIFIC_VALUE, "Write width must be specifc value!");

	choice.readWidth = request.readWidth.value;
	choice.writeWidth = request.writeWidth.value;

	switch (request.readDepth.choice) {
		case Preference::MIN_VALUE: 
			choice.readDepth = utils::nextPow2(std::max<size_t>(512, request.readDepth.value));
			if (choice.readDepth > 8192)
				choice.readDepth = request.readDepth.value; // can't do it (without concatenating fifos)
		break;
		case Preference::MAX_VALUE:
			choice.readDepth = std::min<size_t>(request.readDepth.value, 512);
		break;
		case Preference::SPECIFIC_VALUE:
			choice.readDepth = request.readDepth.value;
		break;
		default:
			choice.readDepth = 512;
	}

	choice.singleClock = request.singleClock.resolveSimpleDefault(true);

	choice.latency_writeToEmpty = request.latency_writeToEmpty.resolveToPreferredMinimum(1);
	choice.latency_readToFull = request.latency_readToFull.resolveToPreferredMinimum(1);
	choice.latency_writeToAlmostEmpty = request.latency_writeToAlmostEmpty.resolveToPreferredMinimum(1);
	choice.latency_readToAlmostFull = request.latency_readToAlmostFull.resolveToPreferredMinimum(1);

	HCL_ASSERT_HINT(choice.singleClock, "Dual clock not yet implemented!");

	return choice;
}


}
