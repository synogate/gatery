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

    private:
        Ui::MainWindowSimulate m_ui;
        
        void reccurFillTreeWidget(QTreeWidgetItem *item, core::hlim::NodeGroup *nodeGroup);
};

}

#endif // MAINWINDOWSIMULATE_H
