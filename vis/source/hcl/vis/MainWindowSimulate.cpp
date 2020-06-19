#include "MainWindowSimulate.h"

#include "Node_Signal.h"
#include "Node_ElementaryOp.h"


#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/utils/Range.h>


#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/stacktrace.hpp>

#include <QFile>
#include <QTextStream>
#include <QTextBlock>
#include <QProgressDialog>
#include <QTimer>

#include <sstream>

boost::filesystem::path shortenPath(const boost::filesystem::path &path)
{
    return boost::filesystem::relative(path, boost::filesystem::current_path());
}

std::string formatStackFrame(const boost::stacktrace::frame &frame) 
{
    return (boost::format("%s (%d): %s") % shortenPath(boost::filesystem::path(frame.source_file())).string() % frame.source_line() % frame.name()).str();
}


namespace hcl::vis {


MainWindowSimulate::MainWindowSimulate(QWidget *parent, core::hlim::Circuit &circuit)
    : QMainWindow(parent), m_circuit(circuit)
{
    m_ui.setupUi(this);
    
    m_ui.toolButton_StepForward->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_MediaPlay));
    m_ui.toolButton_FastForward->setIcon(m_ui.toolButton_FastForward->style()->standardIcon(QStyle::SP_MediaSeekForward));
    m_ui.toolButton_Pause->setIcon(m_ui.toolButton_Pause->style()->standardIcon(QStyle::SP_MediaPause));
    m_ui.toolButton_Pause->setEnabled(false);
    m_ui.toolButton_Reset->setIcon(m_ui.toolButton_Reset->style()->standardIcon(QStyle::SP_BrowserReload));
    
    
    m_simulator.compileProgram(m_circuit);
    m_simControl.bindSimulator(&m_simulator);
    
    
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

    QObject::connect(m_ui.toolButton_FastForward, &QToolButton::pressed,
                     this, &MainWindowSimulate::ontoolButton_FastForward_pressed);
    QObject::connect(m_ui.toolButton_Pause, &QToolButton::pressed,
                     this, &MainWindowSimulate::ontoolButton_Pause_pressed);
    QObject::connect(m_ui.toolButton_StepForward, &QToolButton::pressed,
                     this, &MainWindowSimulate::ontoolButton_StepForward_pressed);
    QObject::connect(m_ui.toolButton_Reset, &QToolButton::pressed,
                     this, &MainWindowSimulate::ontoolButton_Reset_pressed);
    
    
    m_syntaxHighlighter.reset(new CHCLSyntaxHighlighter(m_ui.textEdit_sourceView->document()));


    m_bitmapImage = QImage(1, 1, QImage::Format_Indexed8);
    m_bitmapGraphicsItem = new QGraphicsPixmapItem(QPixmap::fromImage(m_bitmapImage));
    m_ui.graphicsView_bitmapView_graphics->setScene(new QGraphicsScene());
    m_ui.graphicsView_bitmapView_graphics->scene()->addItem(m_bitmapGraphicsItem);

    m_ui.graphicsView_bitmapView_graphics->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_ui.graphicsView_bitmapView_graphics->setDragMode(QGraphicsView::ScrollHandDrag);
    m_ui.graphicsView_bitmapView_graphics->scale(10.0f, 10.0f);
}

MainWindowSimulate::~MainWindowSimulate()
{
}

void MainWindowSimulate::updateBitmap()
{
    if (m_bitmapNode != nullptr) {
        unsigned width = 32;
        
        auto state = m_simulator.getValueOfInternalState(m_bitmapNode, 0);
        
        unsigned rows = (state.size() + width-1)/width;
        
        if (m_bitmapImage.width() != width || m_bitmapImage.height() != rows) {
            m_bitmapImage = QImage(width, rows, QImage::Format_Indexed8);
            m_bitmapImage.setColor(0, qRgb(255, 255, 255));
            m_bitmapImage.setColor(1, qRgb(0, 0, 0));
            m_bitmapImage.setColor(2, qRgb(255, 0, 0));
            m_bitmapImage.setColor(3, qRgb(128, 128, 128));
        }
        
        for (auto y : utils::Range(rows))
            for (auto x : utils::Range(width)) {
                size_t idx = x+y*width;
                if (idx >= state.size())
                    m_bitmapImage.setPixel(x, y, 3);
                else {
                    if (!state.get(core::sim::DefaultConfig::DEFINED, idx))
                        m_bitmapImage.setPixel(x, y, 2);
                    else
                        if (state.get(core::sim::DefaultConfig::VALUE, idx))
                            m_bitmapImage.setPixel(x, y, 0);
                        else
                            m_bitmapImage.setPixel(x, y, 1);
                }
            }
        
        m_bitmapGraphicsItem->setPixmap(QPixmap::fromImage(m_bitmapImage));
    }
}


void MainWindowSimulate::reccurFillTreeWidget(QTreeWidgetItem *item, core::hlim::NodeGroup *nodeGroup)
{
    m_item2NodeGroup[item] = nodeGroup;
    item->setText(0, QString::fromUtf8(nodeGroup->getName().c_str()));
    
    std::vector<core::hlim::NodeGroup*> groupStack = { nodeGroup };
    
    while (!groupStack.empty()) {
        core::hlim::NodeGroup *group = groupStack.back();
        groupStack.pop_back();
        
        for (auto &subGroup : group->getChildren()) {
            if (subGroup->getGroupType() == core::hlim::NodeGroup::GRP_ENTITY) {
                QTreeWidgetItem *newItem;
                item->addChild(newItem = new QTreeWidgetItem());
                reccurFillTreeWidget(newItem, subGroup.get());
            } else
                groupStack.push_back(subGroup.get());
        }
    }
}

void MainWindowSimulate::treeWidget_graphHierarchy_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    if (current != nullptr)
        switchToGroup(m_item2NodeGroup[current]);
}

void MainWindowSimulate::switchToGroup(core::hlim::NodeGroup *nodeGroup)
{
    QProgressDialog progress("Layouting...", "Cancel", 0, 1000, this);
    progress.setMinimumDuration(0);
    progress.setWindowModality(Qt::WindowModal);
    progress.setValue(0);

    m_ui.textEdit_log->append(QString::fromUtf8((std::string("Showing node group '") + nodeGroup->getName() + "'").c_str()));
    
    m_ui.circuitView->render(m_circuit, nodeGroup, [&progress](float p){
        progress.setValue(p * 1000);
        if (progress.wasCanceled())
            ; // haha, nice try
        QCoreApplication::processEvents();
    });
    
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
        
    updateSignalValues();
        
}

void MainWindowSimulate::onCircuitViewElementsClicked(const std::set<BaseGraphicsComposite*> &elements)
{
    for (auto p : elements) {
        Node_ElementaryOp *elemOpNode = dynamic_cast<Node_ElementaryOp*>(p);
        if (elemOpNode != nullptr) {
            m_bitmapNode = elemOpNode->getHlimNode();
            updateBitmap();
            break;
        }            
    }    
    
    core::hlim::BaseNode *firstNode = nullptr;
    Node_Signal *signalNode = nullptr;
    for (auto p : elements) {
        signalNode = dynamic_cast<Node_Signal*>(p);
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
        
        if (signalNode != nullptr) {
            m_ui.tableWidget_signals->selectRow(m_signalNode2TableRow[signalNode]);
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

void MainWindowSimulate::updateSignalValues()
{
    for (auto pair : m_signalNode2TableRow) {
        core::sim::DefaultBitVectorState state = m_simulator.getValueOfOutput({.node = pair.first->getHlimNode(), .port = 0ull});
        if (state.size() == 0)
            m_ui.tableWidget_signals->setItem(pair.second, 1, new QTableWidgetItem(QString::fromUtf8("undefined")));
        else {
            std::stringstream bitString;
            for (auto i : utils::Range(state.size()))
                if (!state.get(core::sim::DefaultConfig::DEFINED, state.size()-1-i))
                    bitString << "?";
                else
                    if (state.get(core::sim::DefaultConfig::VALUE, state.size()-1-i))
                        bitString << "1";
                    else
                        bitString << "0";
            m_ui.tableWidget_signals->setItem(pair.second, 1, new QTableWidgetItem(QString::fromUtf8(bitString.str().c_str())));
        }
    }
}


void MainWindowSimulate::onRunSimulation()
{
    float frequency = m_ui.doubleSpinBox_simulationFrequency->value();
    
    unsigned iters = 1;
    float delay = 1000.0f / frequency;
    while (delay < 200) {
        iters *= 2;
        delay *= 2;
    }

    for (auto iter : utils::Range(iters))
        m_simulator.advanceAnyTick();
    updateSignalValues();
    updateBitmap();
    
    if (m_simulationRunning)
        QTimer::singleShot((unsigned)delay, this, SLOT(onRunSimulation()));
}


void MainWindowSimulate::ontoolButton_FastForward_pressed()
{
    m_simulationRunning = true;
    m_ui.toolButton_FastForward->setEnabled(false);
    m_ui.toolButton_StepForward->setEnabled(false);
    m_ui.toolButton_Pause->setEnabled(true);
    onRunSimulation();
}

void MainWindowSimulate::ontoolButton_Pause_pressed()
{
    m_simulationRunning = false;
    m_ui.toolButton_FastForward->setEnabled(true);
    m_ui.toolButton_StepForward->setEnabled(true);
    m_ui.toolButton_Pause->setEnabled(false);
}

void MainWindowSimulate::ontoolButton_StepForward_pressed()
{
    //m_simControl.advanceAnyTick();
    m_simulator.advanceAnyTick();
    updateSignalValues();
    updateBitmap();
}

void MainWindowSimulate::ontoolButton_Reset_pressed()
{
    m_simulator.reset();
    updateSignalValues();
    updateBitmap();
}

}
