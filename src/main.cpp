#include <QApplication>
#include "main_widget.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    MainWidget w;
    w.setWindowTitle("SimParamEditor");
    w.resize(1200, 800);
    w.show();

    return app.exec();
}


