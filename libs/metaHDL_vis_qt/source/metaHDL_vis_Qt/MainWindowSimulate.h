#ifndef MAINWINDOWSIMULATE_H
#define MAINWINDOWSIMULATE_H

#include <ui_MainWindowSimulate.h>

#include <metaHDL_core/hlim/Circuit.h>


namespace mhdl::vis {


class MainWindowSimulate : public QMainWindow
{
    Q_OBJECT

    public:
        explicit MainWindowSimulate(QWidget *parent, core::hlim::Circuit &circuit);
        ~MainWindowSimulate();
    
    private slots:
        void treeWidget_graphHierarchy_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

    private:
        Ui::MainWindowSimulate m_ui;
        core::hlim::Circuit &m_circuit;
        std::map<QTreeWidgetItem *, core::hlim::NodeGroup *> m_item2NodeGroup;
        
        void reccurFillTreeWidget(QTreeWidgetItem *item, core::hlim::NodeGroup *nodeGroup);
};

}

#endif // MAINWINDOWSIMULATE_H
