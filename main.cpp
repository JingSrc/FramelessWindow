#include "MainWindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    w.setWindowIcon(QIcon(":/MainWindow/icon/window_close.svg"));

    return a.exec();
}
