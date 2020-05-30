#pragma once
#include <cstdint>

namespace vpi_host
{
	class ClockGenerator
	{
	public:
		ClockGenerator(uint64_t clockSimInterval, void* vpiNetHandle);
		ClockGenerator(ClockGenerator&&) = delete;
		ClockGenerator(const ClockGenerator&) = delete;
		~ClockGenerator();

		void onTimeInterval();

	private:
		void* m_VpiNet;
		void* m_VpiCallback;
		uint64_t m_Interval;
	};

}