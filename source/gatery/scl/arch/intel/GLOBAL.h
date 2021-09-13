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

#include <gatery/frontend/Bit.h>

#include <gatery/frontend/tech/TechnologyMappingPattern.h>
#include <gatery/hlim/supportNodes/Node_External.h>

namespace gtry::scl::arch::intel {

class GLOBAL : public gtry::hlim::Node_External
{
    public:
        GLOBAL();

		void connectInput(const Bit &bit);

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

        virtual std::string attemptInferOutputName(size_t outputPort) const override;
    protected:
		void connectInput(hlim::NodePort nodePort);
};


class GLOBALPattern : public TechnologyMappingPattern
{
	public:
		virtual ~GLOBALPattern() = default;

		virtual bool scopedAttemptApply(hlim::NodeGroup *nodeGroup) const override;
	protected:
};

}
