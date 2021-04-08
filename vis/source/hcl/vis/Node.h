/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include "BaseGraphicsComposite.h"

#include <hcl/hlim/NodeIO.h>


#include <QGraphicsItem>

namespace hcl::vis {

class CircuitView;    
    
class Node : public BaseGraphicsComposite
{
    public:
        struct Port {
            std::string name;
            QGraphicsItem *graphicsItem = nullptr;
            core::hlim::NodePort producer;
        };
        
        
        Node(CircuitView *circuitView);

        enum { Type = UserType + 1 };
        int type() const override { return Type; }

        inline const std::vector<Port> &getInputPorts() const { return m_inputPorts; }
        inline const std::vector<Port> &getOutputPorts() const { return m_outputPorts; }
                
    protected:
        CircuitView *m_circuitView;
        std::string m_name;
        QGraphicsItem *m_background = nullptr;
        QGraphicsItem *m_interior = nullptr;
        std::vector<Port> m_inputPorts;
        std::vector<Port> m_outputPorts;
        
        void createDefaultGraphics(float width);
        
        QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
};

}
