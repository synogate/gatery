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
#pragma once

#include <vector>
#include <memory>

namespace gtry::hlim {
	class NodeGroup;
	class Circuit;

	class TechnologyMappingPattern
	{
		public:
			enum Priority {
				OVERRIDE = 0,
				TECH_MAPPING = 1'000,
				EXPORT_LANGUAGE_MAPPING = 1'000'000,
			};

			TechnologyMappingPattern();
			virtual ~TechnologyMappingPattern() = default;

			virtual bool attemptApply(Circuit &circuit, hlim::NodeGroup *nodeGroup) const = 0;

			inline size_t getPriority() const { return m_priority; }
			/// This is a work around for now to allow tech mappings to run before any register retiming and insert their own negative registers and pipelining hints.
			inline bool runPreOptimization() const { return m_runPreOptimization; }
		protected:
			size_t m_priority = TECH_MAPPING + 100;
			bool m_runPreOptimization = false;
	};

	class TechnologyMapping
	{
		public:
			TechnologyMapping();

			void addPattern(std::unique_ptr<TechnologyMappingPattern> pattern);

			void apply(Circuit &circuit, hlim::NodeGroup *nodeGroup, bool preOptimization) const;
		protected:
			std::vector<std::unique_ptr<TechnologyMappingPattern>> m_patterns;
	};


}
