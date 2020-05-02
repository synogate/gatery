#include "MainWindowSimulate.h"


MainWindowSimulate::MainWindowSimulate(QWidget *parent)
    : QMainWindow(parent)
{
    m_ui.setupUi(this);
    
    m_ui.toolButton_StepForward->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_MediaPlay));
    m_ui.toolButton_FastForward->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_MediaSeekForward));
    m_ui.toolButton_Pause->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_MediaPause));
    m_ui.toolButton_Reset->setIcon(m_ui.toolButton_StepForward->style()->standardIcon(QStyle::SP_BrowserReload));
    
}

MainWindowSimulate::~MainWindowSimulate()
{
}
