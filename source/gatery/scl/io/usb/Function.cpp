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
#include "gatery/pch.h"
#include "Function.h"

#include "../../Counter.h"
#include "../../utils/OneHot.h"

gtry::scl::usb::Function::Function() :
	m_area{"usbFunction", true}
{
	m_area.leave();
}

void gtry::scl::usb::Function::setup(Phy& phy)
{
	auto scope = m_area.enter();

	phy.setup(Phy::OpMode::FullSpeedFunction);
	ClockScope clk{ phy.clock() };

	m_phy.checkRxAppendTx(phy.tx(), gtry::reg(phy.rx()));
	m_rxStatus = gtry::reg(phy.status());
	m_clock = phy.clock();

	m_state.init(State::waitForToken);
	m_state.setName("m_state");

	HCL_NAMED(m_phy);
	HCL_NAMED(m_rxStatus);

	m_phy.tx.valid = '0';
	m_phy.tx.error = '0';
	m_phy.tx.data = 0;

	if (DeviceDescriptor* dev = m_descriptor.device())
		m_maxPacketSize = dev->MaxPacketSize;
	m_packetLen = BitWidth::last(m_maxPacketSize);

	generateFunctionReset();
	generateDescriptorRom();
	generateHandshakeFsm();
	generateInitialFsm();
	generateCapturePacket();
	generateRxStream();
	HCL_NAMED(m_phy);

	m_address = reg(m_address, 0);
	HCL_NAMED(m_address);
}

void gtry::scl::usb::Function::addClassSetupHandler(std::function<Bit(const SetupPacket&)> handler)
{
	m_classHandler.push_back(handler);
}

void gtry::scl::usb::Function::attachRxFifo(TransactionalFifo<StreamData>& fifo, uint16_t endPointMask)
{
	auto scope = m_area.enter("RxFifoInterface");
	ClockScope clk{ clock() };

	Bit validEp = '0';
	for(size_t i = 0; i < 16; ++i)
	{
		// do not check mask directly to reduce fanin
		if((endPointMask & (1 << i)) != 0)
			validEp |= m_endPoint == i;
	}
	HCL_NAMED(validEp);

	IF(validEp)
	{
		m_rx.ready = !fifo.full();

		IF(m_rx.valid)
			fifo.push({
				.data = m_rx.data,
				.endPoint = m_rx.endPoint,
		});

		IF(m_rx.eop)
		{
			IF(m_rx.error)
				fifo.rollbackPush();
			ELSE
				fifo.commitPush(2); // cutoff crc
		}
	}
}

void gtry::scl::usb::Function::attachTxFifo(TransactionalFifo<StreamData>& fifo, uint16_t endPointMask)
{
	auto scope = m_area.enter("TxFifoInterface");
	ClockScope clk{ clock() };

	Bit validEp = '0';
	for(size_t i = 0; i < 16; ++i)
	{
		// do not check mask directly to reduce fanin
		if((endPointMask & (1 << i)) != 0)
			validEp |= m_endPoint == i;
	}
	HCL_NAMED(validEp);

	IF(validEp)
	{
		m_tx.valid = !fifo.empty();

		auto txBuffer = fifo.peek();
		m_tx.data = txBuffer.data;
		m_tx.endPoint = txBuffer.endPoint;

		IF(m_tx.ready)
			fifo.pop();
		IF(m_tx.commit)
			fifo.commitPop();
		IF(m_tx.rollback)
			fifo.rollbackPop();
	}
	setName(m_tx, "tx");
}

void gtry::scl::usb::Function::generateCapturePacket()
{
	const PhyRxStream& rx = m_phy.rx;
	const PhyTxStream& tx = m_phy.tx;

	IF(rx.valid & rx.sop)
		m_pid = rx.data.lower(4_b);
	IF(rx.eop)
		m_pid = 0;
	m_pid = reg(m_pid);
	HCL_NAMED(m_pid);

	IF(rx.valid | (tx.valid & tx.ready))
	{
		IF(m_packetLen != m_packetLen.width().last())
			m_packetLen += 1;
	}
	IF(rx.valid & rx.sop)
		m_packetLen = 0;
	IF(m_state.current() == State::sendDataPid)
		m_packetLen = 0;

	m_packetLen = reg(m_packetLen);
	HCL_NAMED(m_packetLen);

	IF(rx.valid & m_packetLen < 8)
	{
		m_packetData >>= 8;
		m_packetData.upper(8_b) = rx.data;
	}
	m_packetData = reg(m_packetData);
	HCL_NAMED(m_packetData);
}

void gtry::scl::usb::Function::generateInitialFsm()
{
	const PhyRxStream& rx = m_phy.rx;

	IF(m_state.current() == State::waitForToken)
	{
		IF(rx.eop & !rx.error)
		{
			IF(m_pid.lower(2_b) == 1) // token pid
			{
				TokenPacket token;
				unpack(m_packetData.upper(16_b).lower(11_b), token);

				IF(m_pid.upper(2_b) == 1) // sof
				{
					m_frameId = m_packetData.upper(16_b).lower(11_b);
				}
				ELSE IF(token.address == m_address) // in, out, setup for us
				{
					m_endPoint = token.endPoint;
					m_endPointMask = scl::decoder(m_endPoint);

					IF(m_pid.upper(2_b) == 3) // setup
					{
						m_state = State::waitForSetup;
						m_nextOutDataPid |= m_endPointMask;
						m_nextInDataPid &= ~m_endPointMask;
					}
					IF(m_pid.upper(2_b) == 2 & m_endPoint == 0) // in setup
					{
						m_state = State::sendDataPid;
						m_sendDataState = State::sendSetupData;
					}
					IF(m_pid.upper(2_b) == 0 & m_endPoint == 0) // out setup
					{
						m_state = State::recvSetupData;
					}

					IF(m_endPoint != 0)
					{
						IF(m_pid.upper(2_b) == 2) // in
						{
							IF(m_tx.valid & m_tx.endPoint == m_endPoint)
							{
								m_sendDataState = State::sendData;
								m_state = State::sendDataPid;
							}
							ELSE
							{
								sendHandshake(Handshake::NAK);
							}
						}
						IF(m_pid.upper(2_b) == 0) // out
						{
							m_state = State::recvDataPid;
						}
					}
				}
			}
		}
	}
	m_endPoint = reg(m_endPoint);
	HCL_NAMED(m_endPoint);
	m_endPointMask = reg(m_endPointMask);
	HCL_NAMED(m_endPointMask);

	m_frameId = reg(m_frameId);
	HCL_NAMED(m_frameId);
	m_sendDataState = reg(m_sendDataState);
	HCL_NAMED(m_sendDataState);

	IF(m_state.current() == State::waitForSetup)
	{
		IF(rx.eop)
		{
			m_state = State::waitForToken;

			IF(!rx.error & m_pid == 3) // data0
			{
				SetupPacket setup;
				unpack(m_packetData, setup);
				sendHandshake(Handshake::STALL);
				m_descLength = 0; // zero length status stage

				IF(setup.requestType(5, 2_b) == 0) // Type Standard
				{
					IF(setup.request == (size_t)SetupRequest::GET_STATUS)
					{
						m_descAddress = 14;
						m_descLength = 2;
						sendHandshake(Handshake::ACK);
					}

					IF(setup.request == (size_t)SetupRequest::CLEAR_FEATURE)
					{
						sendHandshake(Handshake::ACK);
					}

					IF(setup.request == (size_t)SetupRequest::SET_FEATURE)
					{
						IF(setup.wValue == 0)
							sendHandshake(Handshake::ACK);
					}

					IF(setup.requestType.lower(5_b) == 0) // DEVICE
					{

						IF(setup.request == (size_t)SetupRequest::GET_DESCRIPTOR)
						{
							size_t offset = 16;
							for(const DescriptorEntry& e : m_descriptor.entries())
							{
								if(e.type() < 4)
								{
									IF(setup.wValue == (((size_t)e.type() << 8) | e.index))
									{
										m_descAddress = offset;

										if(e.type() == 2) // configuration
											// transfer all sub descriptors
											m_descLength = (size_t)e.data[2] | (size_t(e.data[3]) << 8);
										else
											m_descLength = e.data.size();
										sendHandshake(Handshake::ACK);
									}
								}
								offset += e.data.size();
							}
						}

						IF(setup.request == (size_t)SetupRequest::SET_ADDRESS)
						{
							m_newAddress = setup.wValue.lower(7_b);
							sendHandshake(Handshake::ACK);
						}

						IF(setup.request == (size_t)SetupRequest::GET_CONFIGURATION)
						{
							m_descAddress = zext(m_configuration);
							m_descLength = 1;
							sendHandshake(Handshake::ACK);
						}

						IF(setup.request == (size_t)SetupRequest::SET_CONFIGURATION)
						{
							Bit validConfig = '0';
							for(const DescriptorEntry& e : m_descriptor.entries())
							{
								if(e.type() == 2)
									IF(e.data[5] == setup.wValue)
										validConfig = '1';
							}
							HCL_NAMED(validConfig);

							IF(validConfig)
							{
								m_nextOutDataPid.upper(15_b) = 0;
								m_nextInDataPid.upper(15_b) = 0;
								m_configuration = setup.wValue.lower(4_b);
								sendHandshake(Handshake::ACK);
							}
						}
					}
				}
				
				IF(setup.requestType(5, 2_b) == 1) // Type Class
				{
					Bit handeled = '0';
					for(auto& h : m_classHandler)
						handeled |= h(setup);
					HCL_NAMED(handeled);

					IF(handeled)
						sendHandshake(Handshake::ACK);
				}
				
				IF(zext(m_descLength) > setup.wLength)
					m_descLength = setup.wLength.lower(m_descLength.width());
			}
		}
	}
	m_newAddress = reg(m_newAddress, 0);
	HCL_NAMED(m_newAddress);
	m_configuration = reg(m_configuration, 0);
	HCL_NAMED(m_configuration);

	IF(m_state.current() == State::sendDataPid)
	{
		IF(m_rxIdle)
		{
			m_phy.tx.valid = '1';
			m_phy.tx.data = "b11000011";
			IF((m_nextOutDataPid & m_endPointMask) != 0)
				m_phy.tx.data = "b01001011";
			IF(m_phy.tx.ready)
				m_state = m_sendDataState;
		}

		m_descAddressActive = m_descAddress;
		m_descLengthActive = m_descLength;
	}
	m_nextOutDataPid = reg(m_nextOutDataPid, 0);
	HCL_NAMED(m_nextOutDataPid);
	
	m_descAddressActive = reg(m_descAddressActive);
	HCL_NAMED(m_descAddressActive);
	m_descLengthActive = reg(m_descLengthActive);
	HCL_NAMED(m_descLengthActive);
	m_descAddress = reg(m_descAddress);
	HCL_NAMED(m_descAddress);
	m_descLength = reg(m_descLength);
	HCL_NAMED(m_descLength);

	IF(m_state.current() == State::sendSetupData)
	{
		IF(m_descLengthActive != 0 & m_packetLen != m_maxPacketSize)
		{
			PhyTxStream& tx = m_phy.tx;
			tx.valid = '1';
			tx.data = m_descData;

			IF(tx.ready)
			{
				m_descAddressActive += 1;
				m_descLengthActive -= 1;
			}
		}
		ELSE
		{
			m_state = State::waitForSetupAck;
		}
	}

	HCL_NAMED(m_tx);
	m_tx.ready = '0';
	IF(m_state.current() == State::sendData)
	{
		PhyTxStream& tx = m_phy.tx;

		IF(m_tx.valid & m_tx.endPoint == m_endPoint & m_packetLen != m_maxPacketSize)
		{
			m_tx.ready = tx.ready;
			tx.valid = '1';
			tx.data = m_tx.data;
		}
		ELSE
		{
			m_state = State::waitForSetupAck;
		}
	}

	IF(m_state.current() == State::recvDataPid)
	{
		const PhyRxStream& rx = m_phy.rx;
		IF(rx.sop & rx.valid)
		{
			const UInt expectedPid = cat(m_nextInDataPid[m_endPoint], '0', '1', '1');
			const UInt resendPid = cat(~expectedPid.msb(), '0', '1', '1');

			IF(rx.data == cat(~expectedPid, expectedPid))
				m_state = State::recvData;
			ELSE IF(rx.data != cat(~resendPid, resendPid))
				m_state = State::waitForToken;
			// else stay in state and ack resend
		}

		IF(rx.eop)
		{
			// ack resend
			m_state = State::waitForToken;
			sendHandshake(Handshake::ACK);
		}
	}

	IF(m_state.current() == State::recvData)
	{
		IF(m_phy.rx.eop)
		{
			m_state = State::waitForToken;

			IF(!m_phy.rx.error)
			{
				IF(!m_rxReadyError)
				{
					m_nextInDataPid ^= m_endPointMask;
					sendHandshake(Handshake::ACK);
				}
				ELSE
				{
					sendHandshake(Handshake::NAK);
				}
			}
		}
	}

	m_tx.commit = '0';
	m_tx.rollback = '0';
	IF(m_state.current() == State::waitForSetupAck)
	{
		IF(m_phy.rx.eop)
		{
			m_state = State::waitForToken;

			IF(!m_phy.rx.error & m_pid == 2) // ack
			{
				m_nextOutDataPid ^= m_endPointMask;

				// commit progress
				m_descAddress = m_descAddressActive;
				m_descLength = m_descLengthActive;
				m_address = m_newAddress;

				m_tx.commit = '1';
			}
			ELSE
			{
				m_tx.rollback = '1';
			}
		}
	}
	HCL_NAMED(m_tx);
	m_tx.valid = '0';
	m_tx.data = ConstUInt(8_b);
	m_tx.endPoint = ConstUInt(4_b);

	IF(m_state.current() == State::recvSetupData)
	{
		IF(m_phy.rx.eop)
		{
			m_state = State::waitForToken;

			IF(!m_phy.rx.error & m_pid.lower(2_b) == 3) // data pid
			{
				sendHandshake(Handshake::ACK);
			}
		}
	}

	m_nextInDataPid = reg(m_nextInDataPid);
	HCL_NAMED(m_nextInDataPid);
}

void gtry::scl::usb::Function::generateHandshakeFsm()
{
	size_t clockRatio = (ClockScope::getClk().absoluteFrequency().numerator() + 12'000'000 - 1) / 12'000'000;
	Counter rxIdleCounter{ clockRatio };
	IF(!rxIdleCounter.isLast())
		rxIdleCounter.inc();
	IF(m_phy.rx.eop)
		rxIdleCounter.reset();
	m_rxIdle = rxIdleCounter.isLast();

	m_sendHandshake = reg(m_sendHandshake, 0);
	HCL_NAMED(m_sendHandshake);

	enum class HandshakeState
	{
		idle,
		send,
	};
	Reg<Enum<HandshakeState>> handshakeState{ HandshakeState::idle };
	handshakeState.setName("handshakeState");

	IF(handshakeState.current() == HandshakeState::idle)
	{
		IF(m_rxIdle & m_sendHandshake != 0)
		{
			handshakeState = HandshakeState::send;
		}
	}

	IF(handshakeState.current() == HandshakeState::send)
	{
		m_phy.tx.data = "b11010010"; // ACK
		IF(m_sendHandshake == 1 + (size_t)Handshake::NAK)
			m_phy.tx.data = "b01011010";
		IF(m_sendHandshake == 1 + (size_t)Handshake::STALL)
			m_phy.tx.data = "b00011110";

		m_phy.tx.valid = '1';

		IF(m_phy.tx.ready)
		{
			m_sendHandshake = 0;
			handshakeState = HandshakeState::idle;
		}
	}
}

void gtry::scl::usb::Function::generateDescriptorRom()
{
	std::vector<uint8_t> data(16, 0);

	// add some constants for control queries
	for(uint8_t i = 0; i < 14; ++i)
		data[i] = i;

	// control transfer code expects descriptors at offset 16
	for(const DescriptorEntry& d : m_descriptor.entries())
		data.insert(data.end(), d.data.begin(), d.data.end());

	Memory<UInt> descMem{ data.size(), 8_b};
	m_descAddress		= descMem.addressWidth();
	m_descAddressActive	= descMem.addressWidth();
	m_descLength		= descMem.addressWidth();
	m_descLengthActive	= descMem.addressWidth();

	descMem.fillPowerOnState(sim::createDefaultBitVectorState(data.size(), data.data()));
	m_descData = reg(descMem[m_descAddressActive]);
	HCL_NAMED(m_descData);
}

void gtry::scl::usb::Function::generateRxStream()
{
	auto scope = m_area.enter("rxStream");

	Bit functionStream = m_state.current() == State::recvData & m_configuration != 0;
	HCL_NAMED(functionStream);

	Bit sop = !flag(m_phy.rx.valid & functionStream, m_phy.rx.eop);
	HCL_NAMED(sop);

	setName(m_rx.ready, "m_rx_ready");
	m_rxReadyError = flag(m_phy.rx.valid & functionStream & !m_rx.ready, m_rx.eop);
	HCL_NAMED(m_rxReadyError);

	// default sink all unhandeled endpoints
	m_rx.ready = '1';

	m_rx.valid = m_phy.rx.valid & functionStream & !m_rxReadyError;
	m_rx.sop = sop;
	m_rx.data = m_phy.rx.data;
	m_rx.endPoint = m_endPoint;

	m_rx.eop = m_phy.rx.eop & functionStream;
	m_rx.error = m_phy.rx.error | m_rxReadyError;
	HCL_NAMED(m_rx);
}

void gtry::scl::usb::Function::generateFunctionReset()
{
	auto scope = m_area.enter("functionReset");
	UInt s0timer = 10_b;
	
	IF(m_rxStatus.lineState == 0)
		s0timer += 1;
	ELSE
		s0timer = 1;

	s0timer = reg(s0timer, 0);
	HCL_NAMED(s0timer);

	IF(reg(m_rxStatus.sessEnd | s0timer == 0, '0'))
	{
		m_address = 0;
		m_newAddress = 0;
		m_configuration = 0;
	}
}

void gtry::scl::usb::Function::sendHandshake(Handshake type)
{
	m_sendHandshake = (size_t)type + 1;
}
