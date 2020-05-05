#include "MainWindowSimulate.h"

#include "Node_Signal.h"
#include "Node_ElementaryOp.h"


#include <metaHDL_core/hlim/coreNodes/Node_Signal.h>


#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/stacktrace.hpp>

#include <QFile>
#include <QTextStream>
#include <QTextBlock>

boost::filesystem::path shortenPath(const boost::filesystem::path &path)
{
    return boost::filesystem::relative(path, boost::filesystem::current_path());
}

std::string formatStackFrame(const boost::stacktrace::frame &frame) 
{
    return (boost::format("%s (%d): %s") % shortenPath(boost::filesystem::path(frame.source_file())).string() % frame.source_line() % frame.name()).str();
}


namespace mhdl::vis {


MainWindowSimulate::MainWindowSimulate(QWidget *parent, core::hlim::Circuit &circuit)
    : QMainWindow(parent), m_circuit(circuit)
{
    m_ui.setupUi(this);
    
    m_ui.toolButton_StepForward->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_MediaPlay));
    m_ui.toolButton_FastForward->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_MediaSeekForward));
    m_ui.toolButton_Pause->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_MediaPause));
    m_ui.toolButton_Reset->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_BrowserReload));
    
    
    switchToGroup(m_circuit.getRootNodeGroup());
    
    QTreeWidgetItem *rootItem;
    m_ui.treeView_graphHierarchy->addTopLevelItem(rootItem = new QTreeWidgetItem(m_ui.treeView_graphHierarchy));
    
    reccurFillTreeWidget(rootItem, m_circuit.getRootNodeGroup());    
    
    QObject::connect(m_ui.treeView_graphHierarchy, &QTreeWidget::currentItemChanged,
                     this, &MainWindowSimulate::treeWidget_graphHierarchy_currentItemChanged);
    
    QObject::connect(m_ui.circuitView, &CircuitView::onElementsClicked,
                     this, &MainWindowSimulate::onCircuitViewElementsClicked);

    QObject::connect(m_ui.listWidget_stackTraceView, &QListWidget::currentItemChanged,
                     this, &MainWindowSimulate::onlistWidget_stackTraceView_currentItemChanged);

    m_syntaxHighlighter.reset(new CHCLSyntaxHighlighter(m_ui.textEdit_sourceView->document()));
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
        switchToGroup(m_item2NodeGroup[current]);
}

void MainWindowSimulate::switchToGroup(core::hlim::NodeGroup *nodeGroup)
{
    m_ui.textEdit_log->append(QString::fromUtf8((std::string("Showing node group '") + nodeGroup->getName() + "'").c_str()));
    
    m_ui.circuitView->render(m_circuit, nodeGroup);
    
    size_t row = 0;
    m_signalNode2TableRow.clear();
    for (auto p : m_ui.circuitView->getNodes()) {
        Node_Signal *sigNode = dynamic_cast<Node_Signal *>(p);
        if (sigNode != nullptr) {
            m_signalNode2TableRow[sigNode] = row;
            row++;
        }
    }
    
    m_ui.tableWidget_signals->clearContents();
    m_ui.tableWidget_signals->setRowCount(m_signalNode2TableRow.size());
    for (auto pair : m_signalNode2TableRow) {
        m_ui.tableWidget_signals->setItem(pair.second, 0, new QTableWidgetItem(QString::fromUtf8(pair.first->getHlimNode()->getName().c_str())));
        
        const auto &stackTrace = pair.first->getHlimNode()->getStackTrace().getTrace();
        if (stackTrace.size() >= 3)
            m_ui.tableWidget_signals->setItem(pair.second, 2, new QTableWidgetItem(QString::fromUtf8(formatStackFrame(stackTrace[2]).c_str())));
        else
            m_ui.tableWidget_signals->setItem(pair.second, 2, new QTableWidgetItem(QString::fromUtf8("No stack trace")));
        
        auto driver = pair.first->getHlimNode()->getDriver(0);
        if (driver.node != nullptr) {
            const auto &stackTrace = driver.node->getStackTrace().getTrace();
            if (stackTrace.size() >= 3)
                m_ui.tableWidget_signals->setItem(pair.second, 3, new QTableWidgetItem(QString::fromUtf8(formatStackFrame(stackTrace[2]).c_str())));
            else
                m_ui.tableWidget_signals->setItem(pair.second, 3, new QTableWidgetItem(QString::fromUtf8("No stack trace")));
        } else {
            m_ui.tableWidget_signals->setItem(pair.second, 3, new QTableWidgetItem(QString::fromUtf8("No driver")));
        }
    }
        
}

void MainWindowSimulate::onCircuitViewElementsClicked(const std::set<BaseGraphicsComposite*> &elements)
{
    core::hlim::BaseNode *firstNode = nullptr;
    for (auto p : elements) {
        Node_Signal *signalNode = dynamic_cast<Node_Signal*>(p);
        if (signalNode != nullptr) {
            firstNode = signalNode->getHlimNode();
            break;
        }            
        Node_ElementaryOp *elemOpNode = dynamic_cast<Node_ElementaryOp*>(p);
        if (elemOpNode != nullptr) {
            firstNode = elemOpNode->getHlimNode();
            break;
        }            
    }
    
    m_ui.listWidget_stackTraceView->clear();
    m_listWidget_stackTraceView_stackTrace.clear();
    if (firstNode == nullptr) {
        m_ui.label_sourceNodeName->setText(QString::fromUtf8("Source:"));
    } else {
        std::string nodeName = std::string("'") + firstNode->getName() + "' [" +firstNode->getTypeName()+"]";
        
        m_ui.label_sourceNodeName->setText(QString::fromUtf8((std::string("Source of: ") + nodeName).c_str()));
        
        const auto &stackTrace = firstNode->getStackTrace().getTrace();
        for (const auto &s : stackTrace) {
            QListWidgetItem *item = new QListWidgetItem(QString::fromUtf8(formatStackFrame(s).c_str()));
            m_ui.listWidget_stackTraceView->addItem(item);
            m_listWidget_stackTraceView_stackTrace[item] = s;
        }
    }
}

void MainWindowSimulate::onlistWidget_stackTraceView_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    m_ui.textEdit_sourceView->clear();
    if (current == nullptr) {
        m_ui.textEdit_sourceView->setText(QString::fromUtf8("No stack frame selected"));
    } else {
        auto frame = m_listWidget_stackTraceView_stackTrace[current];
        
        QFile file(QString::fromUtf8(frame.source_file().c_str()));
        if (!file.open(QFile::ReadOnly | QFile::Text)) {
            m_ui.textEdit_sourceView->setText(QString::fromUtf8("Could not open source file"));
            m_ui.textEdit_log->append(QString::fromUtf8((std::string("Can not find file ") + frame.source_file()).c_str()));
        } else {
            QTextStream fileStream(&file);
            m_ui.textEdit_sourceView->setText(fileStream.readAll());

            QTextCursor cursor(m_ui.textEdit_sourceView->document()->findBlockByLineNumber(frame.source_line()-2)); // why -2?
            cursor.select(QTextCursor::LineUnderCursor);
            m_ui.textEdit_sourceView->setTextCursor(cursor);
        }
    }
}

}
