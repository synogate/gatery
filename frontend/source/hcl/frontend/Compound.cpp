#include "Compound.h"

namespace hcl::core::frontend
{

	void CompoundVisitor::enterPackStruct()
	{
	}

	void CompoundVisitor::enterPackContainer()
	{
	}

	void CompoundVisitor::leavePack()
	{
	}

	void CompoundVisitor::enter(std::string_view name)
	{
	}

	void CompoundVisitor::leave()
	{
	}

	void CompoundVisitor::operator()(const BVec& a, const BVec& b)
	{
	}

	void CompoundVisitor::operator()(BVec& a)
	{
	}

	void CompoundVisitor::operator()(BVec& a, const BVec& b)
	{
	}

	void CompoundVisitor::operator()(const Bit& a, const Bit& b)
	{
	}

	void CompoundVisitor::operator()(Bit& a)
	{
	}

	void CompoundVisitor::operator()(Bit& a, const Bit& b)
	{
	}

	void CompoundNameVisitor::enter(std::string_view name)
	{
		m_names.push_back(name);
		CompoundVisitor::enter(name);
	}

	void CompoundNameVisitor::leave()
	{
		m_names.pop_back();
		CompoundVisitor::leave();
	}

	std::string CompoundNameVisitor::makeName() const
	{
		std::string name;
		for (std::string_view part : m_names)
		{
			if (!name.empty() && !std::isdigit(part.front()))
				name += '_';
			name += part;
		}
		return name;
	}
}
