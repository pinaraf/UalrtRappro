#include "mainwindow.h"
#include "logindialog.h"

#include <QApplication>
#include <QTranslator>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    if (translator.load(QLocale(), QLatin1String("rappro"), QLatin1String("_"), QLatin1String(":/translations")))
        a.installTranslator(&translator);

    LoginDialog loginDialog;
    if (!loginDialog.exec()) {
        // User cancelled
        return 0;
    }

    // Check login from loginDialog.login() loginDialog.password()

    MainWindow w;
    w.show();

    return a.exec();
}
