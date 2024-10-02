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
#include "Scope.h"

namespace gtry
{

	namespace hlim {
		struct GroupAttributes;
	}

/**
 * @addtogroup gtry_scopes
 * @{
 */


	/**
	 * @brief A chip area for grouping chip elements together (entity in vhdl)
	 * @details The actual area is (usually) created by instantiating a GroupScope variable via Area::enter().
	 * All logic created while that GroupScope variable exists will be placed inside the area.
	 * Areas can be nested.
	 *
	 * Essentially, there are two ways of using areas: Either directly:
	 * @code
	 *	{ // scoping bracket
	 *	Area newArea("ip_core", true);
	 *
	 *	// Build logic that is to be placed in the "ip_core" area
	 *
	 *	} // closing scoping bracket
	 * @endcode
	 * Or by entering the scope manually:
	 * @code
	 *  Area newArea("ip_core", false);
	 *	{ // scoping bracket
	 *	  auto scp = newArea.enter();
	 *
	 *	  // Build logic that is to be placed in the "ip_core" area
	 *
	 *	} // closing scoping bracket
	 *
	 *  // ...
	 *
	 *	{ // scoping bracket
	 *	  auto scp = newArea.enter();
	 *
	 *	  // Build more logic that is to be placed in the "ip_core" area
	 *
	 *	} // closing scoping bracket
	 * @endcode
	 */
	class Area
	{
	public:
		/**
		 * @brief Prepares the area but does not yet enter the area's scope unless specified explicitely.
		 * 
		 * @param name Name of the area
		 * @param instantEnter If true, directly enters the Area's scope which must be left with Area::leave().
		 */
		Area(std::string_view name, bool instantEnter = false);
		
		/// Access to properties (e.g. from config files) that were places for this Area (in the total area hierarchy).
		utils::PropertyTree operator [] (std::string_view key);

		/// Creates a GroupScope which automatically handles entering/leaving this Area
		GroupScope enter() const;

		/**
		 * @brief Creates a new sub-Area and returns GroupScopes which allow directly entering the new sub-Area.
		 * @param subName Name of the new sub-area
		 * @returns A pair of GroupScopes with the first one being the GroupScope of the calling Area and the second one being the GroupScope of the new sub-Area.
		 */
		std::pair<GroupScope, GroupScope> enter(std::string_view subName) const;

		/// If the Area was directly entered in the constructor, this function allows leaving the area.
		void leave() { m_inScope.reset(); }


		inline hlim::NodeGroup* getNodeGroup() { return m_nodeGroup; }

		/**
		 * @brief Store meta information in an area for later pick up by technology replacement routines.
		 * @return The created meta information object, owned by the underlying node group.
		 */
		template<typename MetaType, typename... Args>
		MetaType *createMetaInfo(Args&&... args) {
			metaInfo(std::make_unique<MetaType>(std::forward<Args>(args)...));
			return (MetaType*)metaInfo();
		}

		void metaInfo(std::unique_ptr<hlim::NodeGroupMetaInfo> metaInfo);
		hlim::NodeGroupMetaInfo* metaInfo();

		/**
		 * @brief Whether or not to make this area a partition in a separate file with stable inputs (@see gtry::vhdl::VHDLExport::outputMode).
		 * @details Setting this to true implies that the instantiation is to use component instantiations
		 */
		void setPartition(bool value);
		bool isPartition() const;

		/**
		 * @brief Whether to use entity instantiations or component instantiations on this area.
		 */
		void useComponentInstantiation(bool b);
		bool useComponentInstantiation() const;

		hlim::GroupAttributes &groupAttributes();
		const hlim::GroupAttributes &groupAttributes() const;

		void instanceName(std::string name);
		const std::string &instanceName() const;
		std::string instancePath() const;
	private:
		hlim::NodeGroup* m_nodeGroup;

		std::optional<GroupScope> m_inScope;
	};

/**@}*/
	
}
