#include <QApplication>
#include <QTextCodec>
#include "ui/main_window/main_widget.h"
#include "ui/theme_manager.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    ThemeManager::instance().applyTheme(app, ThemeManager::ThemeVariant::Light);

    MainWidget w;
    w.setWindowTitle("SimParamEditor");
    w.resize(1200, 800);
    w.show();

    return app.exec();
}

