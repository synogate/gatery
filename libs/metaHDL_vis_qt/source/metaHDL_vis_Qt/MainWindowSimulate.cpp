#include "MainWindowSimulate.h"

namespace mhdl::vis {


MainWindowSimulate::MainWindowSimulate(QWidget *parent, core::hlim::Circuit &circuit)
    : QMainWindow(parent), m_circuit(circuit)
{
    m_ui.setupUi(this);
    
    m_ui.toolButton_StepForward->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_MediaPlay));
    m_ui.toolButton_FastForward->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_MediaSeekForward));
    m_ui.toolButton_Pause->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_MediaPause));
    m_ui.toolButton_Reset->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_BrowserReload));
    
    
    //m_ui.circuitView->render(m_circuit, m_circuit.getRootNodeGroup()->getChildren().front()->getChildren().front().get());
    m_ui.circuitView->render(m_circuit, m_circuit.getRootNodeGroup());
    
    QTreeWidgetItem *rootItem;
    m_ui.treeView_graphHierarchy->addTopLevelItem(rootItem = new QTreeWidgetItem(m_ui.treeView_graphHierarchy));
    
    reccurFillTreeWidget(rootItem, m_circuit.getRootNodeGroup());    
    
     QObject::connect(m_ui.treeView_graphHierarchy, &QTreeWidget::currentItemChanged,
                     this, &MainWindowSimulate::treeWidget_graphHierarchy_currentItemChanged);
}

MainWindowSimulate::~MainWindowSimulate()
{
}

void MainWindowSimulate::reccurFillTreeWidget(QTreeWidgetItem *item, core::hlim::NodeGroup *nodeGroup)
{
    m_item2NodeGroup[item] = nodeGroup;
    item->setText(0, QString::fromUtf8(nodeGroup->getName().c_str()));
    for (auto &area : nodeGroup->getChildren())
        for (auto &entity : area->getChildren()) {
            QTreeWidgetItem *newItem;
            item->addChild(newItem = new QTreeWidgetItem());
            reccurFillTreeWidget(newItem, entity.get());
        }
            
    
}

void MainWindowSimulate::treeWidget_graphHierarchy_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    if (current != nullptr)
        m_ui.circuitView->render(m_circuit, m_item2NodeGroup[current]);
}


}
