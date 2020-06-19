#pragma once
#include "vpi_simulation_host.h"
#include <list>

namespace vpi_host
{
	class VpiModule
	{
	public:
		VpiModule();

		const SimInfo& simInfo() const { return m_Info; }
	protected:
		void initPorts();

	private:
		void* m_VpiModule;
		SimInfo m_Info;
		std::vector<void*> m_InputNet;
		std::vector<void*> m_OutputNet;

				
	};
}
