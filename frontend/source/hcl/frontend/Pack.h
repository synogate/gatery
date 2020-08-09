#pragma once
#include "BitVector.h"
#include "Bit.h"

#include <stack>

#include <boost/spirit/home/support/container.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/fwd/accessors.hpp>

namespace hcl::core::frontend
{
	namespace internal
	{
		struct SignalVisitor
		{
			void enter(std::string_view name) {}
			void leave() {}

			void operator () (const BVec& vec) {}
			void operator () (const Bit& vec) {}
		};

		template<typename Visitor, typename Compound>
		void visitCompound(Visitor& v, Compound& signal)
		{
			if constexpr (boost::spirit::traits::is_container<Compound>::value)
			{
				size_t idx = 0;
				std::string idx_string;

				for (auto& item : signal)
				{
					idx_string = std::to_string(idx++);
					v.enter(idx_string);
					visitCompound(v, item);
					v.leave();
				}
			}
			else if constexpr (boost::hana::Struct<Compound>::value)
			{
				boost::hana::for_each(boost::hana::accessors<Compound>(), [&](auto member) {
					v.enter(boost::hana::first(member).c_str());
					visitCompound(v, boost::hana::second(member)(signal));
					v.leave();
					});
			}
			else
			{
				v(signal);
			}
		}

		template<typename Comp>
		std::tuple<size_t, size_t> countAndWidth(const Comp& compound)
		{
			struct WidthVisitor : internal::SignalVisitor
			{
				void operator () (const BVec& vec) { m_totalCount++; m_totalWidth += vec.size(); }
				void operator () (const Bit& vec) { m_totalCount++; m_totalWidth++; }

				size_t m_totalWidth = 0;
				size_t m_totalCount = 0;
			};

			WidthVisitor v;
			internal::visitCompound(v, compound);
			return { v.m_totalCount, v.m_totalWidth };
		}

	}

	template<typename Comp>
	void name(Comp& compound, std::string_view prefix)
	{
		struct NameVisitor
		{
			void enter(std::string_view name) { m_names.push_back(name); }
			void leave() { m_names.pop_back(); }

			void operator () (BVec& vec) { vec.setName(buildName()); }
			void operator () (Bit& vec) { vec.setName(buildName()); }

		private:
			const std::string buildName()
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

			std::vector<std::string_view> m_names;
		};

		NameVisitor v;
		v.enter(prefix);
		internal::visitCompound(v, compound);
	}

	template<typename Comp>
	size_t width(const Comp& compound)
	{
		return get<1>(internal::countAndWidth(compound));
	}
	
	template<typename Comp>
	BVec pack(const Comp& compound)
	{
		struct PackVisitor : internal::SignalVisitor
		{
			PackVisitor(size_t numInputs) {
				m_node = DesignScope::createNode<hlim::Node_Rewire>(numInputs);
				m_node->recordStackTrace();
				m_op.ranges.reserve(numInputs);
			}

			void operator () (const ElementarySignal& vec) 
			{ 
				m_node->connectInput(m_op.ranges.size(), vec.getReadPort());
				
				m_op.ranges.emplace_back(hlim::Node_Rewire::OutputRange{
					.subwidth = vec.getWidth(),
					.source = hlim::Node_Rewire::OutputRange::INPUT,
					.inputIdx = m_op.ranges.size(),
					.inputOffset = 0
				});
				
				m_totalWidth += vec.getWidth();
			}

			hlim::Node_Rewire* m_node;
			hlim::Node_Rewire::RewireOperation m_op;
			size_t m_totalWidth = 0;
		};

		auto [count, width] = internal::countAndWidth(compound);

		PackVisitor v{ count };
		internal::visitCompound(v, compound);
		v.m_node->setOp(v.m_op);
		return hlim::NodePort{ v.m_node, 0 };
	}
	
	template<typename Comp>
	void unpack(Comp& compound, const BVec& vec)
	{
		struct UnpackVisitor : internal::SignalVisitor
		{
			UnpackVisitor(const BVec& out) : m_packed(out) {}

			void operator () (BVec& vec)
			{
//				vec = m_packed(m_totalWidth, vec.size());
				m_totalWidth += vec.size();
			}

			void operator () (Bit& vec)
			{
				vec = m_packed[m_totalWidth];
				m_totalWidth++;
			}

			const BVec& m_packed;
			size_t m_totalWidth = 0;
		};

		HCL_DESIGNCHECK(vec.size() == width(compound));

		UnpackVisitor v{ vec };
		internal::visitCompound(v, compound);
	}


}
