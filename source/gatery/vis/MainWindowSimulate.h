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
#ifndef MAINWINDOWSIMULATE_H
#define MAINWINDOWSIMULATE_H

#include "CHCLSyntaxHighlighter.h"

#include <ui_MainWindowSimulate.h>

#include <hcl/hlim/Circuit.h>
#include <hcl/simulation/ReferenceSimulator.h>
#include <hcl/simulation/SimulatorControl.h>
#include <hcl/simulation/SimulatorCallbacks.h>

#include <boost/stacktrace.hpp>

#include <memory>

namespace hcl::vis {

class Node_Signal;

class MainWindowSimulate : public QMainWindow, hcl::core::sim::SimulatorCallbacks
{
    Q_OBJECT

    public:
        explicit MainWindowSimulate(QWidget *parent, core::hlim::Circuit &circuit);
        ~MainWindowSimulate();

        inline core::sim::ReferenceSimulator& getSimulator() { return m_simulator; }
    
        virtual void onNewTick(const core::hlim::ClockRational &simulationTime) override;
        virtual void onClock(const core::hlim::Clock *clock, bool risingEdge) override;
        virtual void onDebugMessage(const core::hlim::BaseNode *src, std::string msg) override;
        virtual void onWarning(const core::hlim::BaseNode *src, std::string msg) override;
        virtual void onAssert(const core::hlim::BaseNode *src, std::string msg) override;

        void powerOn() { ontoolButton_Reset_pressed(); }
    private slots:
        void treeWidget_graphHierarchy_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
        void onCircuitViewElementsClicked(const std::set<BaseGraphicsComposite*> &elements);
        void onlistWidget_stackTraceView_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
        void ontoolButton_FastForward_pressed();
        void ontoolButton_Pause_pressed();
        void ontoolButton_StepForward_pressed();
        void ontoolButton_Reset_pressed();
        void onRunSimulation();

        
    private:
        Ui::MainWindowSimulate m_ui;
        core::hlim::Circuit &m_circuit;
        core::sim::ReferenceSimulator m_simulator;
        
        std::map<QTreeWidgetItem *, core::hlim::NodeGroup *> m_item2NodeGroup;
        std::map<Node_Signal*, unsigned> m_signalNode2TableRow;
        
        std::map<QListWidgetItem *, boost::stacktrace::frame> m_listWidget_stackTraceView_stackTrace;
        
        std::unique_ptr<CHCLSyntaxHighlighter> m_syntaxHighlighter;
        
        core::hlim::BaseNode *m_bitmapNode = nullptr;
        QGraphicsPixmapItem *m_bitmapGraphicsItem;
        QImage m_bitmapImage;
        
        bool m_simulationRunning = false;
        
        
        void updateBitmap();
        void reccurFillTreeWidget(QTreeWidgetItem *item, core::hlim::NodeGroup *nodeGroup);
        void switchToGroup(core::hlim::NodeGroup *nodeGroup);
        void updateSignalValues();
};

}

#endif // MAINWINDOWSIMULATE_H
