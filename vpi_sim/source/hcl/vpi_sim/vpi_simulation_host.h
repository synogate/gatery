#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace vpi_host
{
	enum class SignalDirection
	{
		none,
		in,
		out
	};

	struct SignalInfo
	{
		std::string name;
		uint32_t width = 0;
		SignalDirection direction = SignalDirection::none;

		template<class Archive>
		void serialize(Archive& ar, const unsigned int)
		{
			ar & name & width & direction;
		}
	};

	struct SimInfo
	{
		std::string rootModule;
		int32_t timeScale = 0;
		std::vector<SignalInfo> input;
		std::vector<SignalInfo> output;

		template<class Archive>
		void serialize(Archive& ar, const unsigned int)
		{
			ar & rootModule & timeScale & input & output;
		}
	};
}
