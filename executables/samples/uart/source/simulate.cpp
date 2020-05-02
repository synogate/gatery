
#include <QApplication>

#include <metaHDL_vis_Qt/MainWindowSimulate.h>

#include <locale.h>

int main(int argc, char *argv[]) { 
    setlocale(LC_ALL, "en_US.UTF-8");


    QApplication a(argc, argv);

    MainWindowSimulate w;
    w.show();

    return a.exec();   
}
