#ifndef MAINWINDOWSIMULATE_H
#define MAINWINDOWSIMULATE_H

#include <ui_MainWindowSimulate.h>

class MainWindowSimulate : public QMainWindow
{
    Q_OBJECT

    public:
        explicit MainWindowSimulate(QWidget *parent = nullptr);
        ~MainWindowSimulate();

    private slots:

    private:
        Ui::MainWindowSimulate m_ui;
};

#endif // MAINWINDOWSIMULATE_H
