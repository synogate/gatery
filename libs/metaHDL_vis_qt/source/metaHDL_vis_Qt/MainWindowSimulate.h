#ifndef MAINWINDOWSIMULATE_H
#define MAINWINDOWSIMULATE_H

#include "CHCLSyntaxHighlighter.h"

#include <ui_MainWindowSimulate.h>

#include <metaHDL_core/hlim/Circuit.h>
#include <metaHDL_core/simulation/ReferenceSimulator.h>
#include <metaHDL_core/simulation/SimulatorControl.h>

#include <boost/stacktrace.hpp>

#include <memory>

namespace mhdl::vis {

class Node_Signal;

class MainWindowSimulate : public QMainWindow
{
    Q_OBJECT

    public:
        explicit MainWindowSimulate(QWidget *parent, core::hlim::Circuit &circuit);
        ~MainWindowSimulate();
    
    private slots:
        void treeWidget_graphHierarchy_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
        void onCircuitViewElementsClicked(const std::set<BaseGraphicsComposite*> &elements);
        void onlistWidget_stackTraceView_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

    private:
        Ui::MainWindowSimulate m_ui;
        core::hlim::Circuit &m_circuit;
        core::sim::ReferenceSimulator m_simulator;
        core::sim::SimulatorControl m_simControl;
        
        std::map<QTreeWidgetItem *, core::hlim::NodeGroup *> m_item2NodeGroup;
        std::map<Node_Signal*, unsigned> m_signalNode2TableRow;
        
        std::map<QListWidgetItem *, boost::stacktrace::frame> m_listWidget_stackTraceView_stackTrace;
        
        std::unique_ptr<CHCLSyntaxHighlighter> m_syntaxHighlighter;
        
        void reccurFillTreeWidget(QTreeWidgetItem *item, core::hlim::NodeGroup *nodeGroup);
        
        void switchToGroup(core::hlim::NodeGroup *nodeGroup);
        
        void updateSignalValues();
};

}

#endif // MAINWINDOWSIMULATE_H
