#pragma once
#include <hcl/hlim/Node.h>
#include <hcl/hlim/NodeIO.h>

#include <string_view>

namespace hcl::core::frontend
{
	class Bit;
	class BVec;
	class BVecSlice;
	template<typename T> class BVecBitProxy;

	class SignalPort
	{
	public:
		size_t getWidth() const { return getConnType().width; }
		const hlim::ConnectionType& getConnType() const { return m_port.node->getOutputConnectionType(m_port.port); }
		const hlim::NodePort& getReadPort() const { return m_port; }

	protected:
		void setPort(hlim::NodePort port) { m_port = port; HCL_ASSERT(m_port.node); }

	private:
		hlim::NodePort m_port;
	};

	class BitSignalPort : public SignalPort
	{
	public:
		BitSignalPort(char);
		BitSignalPort(bool);
		BitSignalPort(const Bit&);
		BitSignalPort(const BVecBitProxy<BVec>&);
	};

	class BVecSignalPort : public SignalPort
	{
	public:
		BVecSignalPort(std::string_view);
		BVecSignalPort(uint64_t);
		BVecSignalPort(const BVec&);
		BVecSignalPort(const BVecSlice&);
	};

}
