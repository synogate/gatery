#pragma once
#include "BitVector.h"
#include "Bit.h"

#include <hcl/hlim/SignalGroup.h>


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
			void enterPackStruct() {}
			void enterPackContainer() {}
			void leavePack() {}

			void enter(std::string_view name) {}
			void leave() {}

			void operator () (const BVec& vec) {}
			void operator () (const Bit& vec) {}
		};

		template<typename Visitor, typename Compound, typename ...Compounds>
		void visitCompound(Visitor& v, Compound& signal, Compounds& ... signals)
		{
			visitCompound(v, signal);
			visitCompound(v, signals...);
		}

		template<typename Visitor, typename Compound>
		void visitCompound(Visitor& v, Compound& signal)
		{
			if constexpr (boost::spirit::traits::is_container<Compound>::value)
			{
                v.enterPackContainer();
				size_t idx = 0;
				std::string idx_string;

				for (auto& item : signal)
				{
					idx_string = std::to_string(idx++);
					v.enter(idx_string);
					visitCompound(v, item);
					v.leave();
				}
				v.leavePack();
			}
			else if constexpr (boost::hana::Struct<Compound>::value)
			{
                v.enterPackStruct();
				boost::hana::for_each(boost::hana::accessors<std::remove_cv_t<Compound>>(), [&](auto member) {
					v.enter(boost::hana::first(member).c_str());
					visitCompound(v, boost::hana::second(member)(signal));
					v.leave();
					});
                v.leavePack();
			}
			else if constexpr (std::is_base_of_v<ElementarySignal, Compound>)
			{
				v(signal);
			}
		}

		template<typename... Comp>
		std::tuple<size_t, size_t> countAndWidth(const Comp& ...compound)
		{
			struct WidthVisitor : internal::SignalVisitor
			{
				void operator () (const BVec& vec) { 
					m_totalCount++; m_totalWidth += vec.size(); 
				}
				void operator () (const Bit& vec) { 
					m_totalCount++; m_totalWidth++; 
				}

				size_t m_totalWidth = 0;
				size_t m_totalCount = 0;
			};

			WidthVisitor v;
			internal::visitCompound(v, compound...);
			return { v.m_totalCount, v.m_totalWidth };
		}

	}

	template<typename Comp>
	void setName(Comp& compound, std::string_view prefix)
	{
		struct NameVisitor : public internal::SignalVisitor
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

	template<typename... Comp>
	size_t width(const Comp& ...compound)
	{
		return std::get<1>(internal::countAndWidth(compound...));
	}
	
	template<typename... Comp>
	BVec pack(const Comp& ...compound)
	{
		struct PackVisitor : internal::SignalVisitor
		{
			PackVisitor(size_t numInputs) {
				m_idx = numInputs - 1;
				m_node = DesignScope::createNode<hlim::Node_Rewire>(numInputs);
				m_node->recordStackTrace();
			}

			void operator () (const ElementarySignal& vec) 
			{ 
				m_node->connectInput(m_idx--, vec.getReadPort());
			}

			hlim::Node_Rewire* m_node;
			size_t m_idx = 0;
		};

		auto [count, width] = internal::countAndWidth(compound...);

		PackVisitor v{ count };
		internal::visitCompound(v, compound...);
		v.m_node->setConcat();
		return SignalReadPort(v.m_node);
	}
	
	template<typename... Comp>
	void unpack(const BVec& vec, Comp& ... compound)
	{
		struct UnpackVisitor : internal::SignalVisitor
		{
			UnpackVisitor(const BVec& out) : 
				m_packed(out),
				m_totalWidth(out.size())
			{}

			void operator () (BVec& vec)
			{
				auto* node = DesignScope::createNode<hlim::Node_Rewire>(1);
				node->recordStackTrace();
				node->connectInput(0, m_packed.getReadPort());
				m_totalWidth -= vec.size();
				node->setExtract(m_totalWidth, vec.getWidth());
			
				vec = BVec{ SignalReadPort(node) };
			}

			void operator () (Bit& vec)
			{
				auto* node = DesignScope::createNode<hlim::Node_Rewire>(1);
				node->recordStackTrace();
				node->connectInput(0, m_packed.getReadPort());
				node->changeOutputType({ hlim::ConnectionType::BOOL });
				m_totalWidth--;
				node->setExtract(m_totalWidth, 1);

				vec = Bit{ SignalReadPort(node) };
			}

			const BVec& m_packed;
			size_t m_totalWidth = 0;
		};

		HCL_DESIGNCHECK(vec.size() == width(compound...));

		UnpackVisitor v{ vec };
		internal::visitCompound(v, compound...);
	}

	
    template<typename Comp>
	void makeSignalGroup(Comp& compound)
	{
		struct Visitor : public internal::SignalVisitor
		{
			void enterPack(hlim::SignalGroup::GroupType groupType) {
				hlim::SignalGroup *sigGroup;
				if (m_sigGroups.empty())
					sigGroup = DesignScope::get()->getCircuit().createSignalGroup(groupType);
				else
					sigGroup = m_sigGroups.back()->addChildSignalGroup(groupType);
				sigGroup->recordStackTrace();
				m_sigGroups.push_back(sigGroup);
			}
			void enterPackStruct() {
				enterPack(hlim::SignalGroup::GroupType::STRUCT);
            }
			void enterPackContainer() {
				enterPack(hlim::SignalGroup::GroupType::ARRAY);
            }
			void leavePack() {
				m_sigGroups.pop_back();                
            }
			
			void operator () (BVec& vec)
			{
                if (!m_sigGroups.empty())
                    vec.addToSignalGroup(m_sigGroups.back());
			}

			void operator () (Bit& bit) 
            {
                if (!m_sigGroups.empty())
                    bit.addToSignalGroup(m_sigGroups.back());
            }

			std::vector<hlim::SignalGroup*> m_sigGroups;
		};

		Visitor v;
		internal::visitCompound(v, compound);
	}

}
