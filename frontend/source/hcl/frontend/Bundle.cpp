#include "Bundle.h"

namespace hcl
{
	Bundle::Item* hcl::Bundle::find(const std::type_info& type, std::string_view name, size_t index)
	{
		for (Item& it : m_member)
		{
			if (it.index == index && it.name == name && it.instance.type() == type)
				return &it;
		}

		return nullptr;
	}

	const Bundle::Item* hcl::Bundle::find(const std::type_info& type, std::string_view name, size_t index) const
	{
		for (const Item& it : m_member)
		{
			if (it.index == index && it.name == name && it.instance.type() == type)
				return &it;
		}

		return nullptr;
	}

}
