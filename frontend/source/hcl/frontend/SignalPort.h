#pragma once
#include <hcl/hlim/Node.h>
#include <hcl/hlim/NodeIO.h>

#include <string_view>
#include <type_traits>

namespace hcl::core::frontend
{
	class Bit;
	class BVec;
	class BVecSlice;
	template<typename T> class BVecBitProxy;

	class SignalPort
	{
	public:
		void setPort(hlim::NodePort port) { m_port = port; }
		void setConnType(const hlim::ConnectionType& type) { m_connType = type; }
		void setName(std::string_view name) { m_name = name; }

		std::string_view getName() const { return m_name; }

		size_t getWidth() const { return getConnType().width; }
		const hlim::ConnectionType& getConnType() const { return m_connType; }
		const hlim::NodePort& getReadPort() const { return m_port; }

	private:
		hlim::NodePort m_port;
		hlim::ConnectionType m_connType;
		std::string_view m_name;
	};

	class BitSignalPort : public SignalPort
	{
	public:
		BitSignalPort(char);
		BitSignalPort(bool);
		BitSignalPort(const Bit&);
		BitSignalPort(const BVecBitProxy<BVec>&);
		BitSignalPort(const BVecBitProxy<const BVec>&);
	};

	class BVecSignalPort : public SignalPort
	{
	public:
		BVecSignalPort(std::string_view);
		BVecSignalPort(const BVec&);
		BVecSignalPort(const BVecSlice&);

		template <typename U = uint64_t, typename = std::enable_if_t<std::is_integral_v<U> & !std::is_same_v<U, char> & !std::is_same_v<U, bool>> >
		BVecSignalPort(U vec) : BVecSignalPort(ConstBVec(vec)) {}
	};

}
