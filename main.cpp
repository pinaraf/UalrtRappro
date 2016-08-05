#include "mainwindow.h"
#include "logindialog.h"

#include <QApplication>
#include <QTranslator>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QSettings>
#include <QInputDialog>
#include <QFileDialog>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setApplicationName("UalrtRappro");
    a.setOrganizationName("UALRT");
    a.setOrganizationDomain("www.ualrt.org");

    QTranslator translator;
    if (translator.load(QLocale(), QLatin1String("rappro"), QLatin1String("_"), QLatin1String(":/translations")))
        a.installTranslator(&translator);

    QSettings settings;

    if (!settings.contains("aerogest/password") || !settings.contains("aerogest/path")) {
        QString aerogestPath = QFileDialog::getOpenFileName(nullptr, QObject::tr("Path of aerogest database"));
        if (aerogestPath.isEmpty())
            return -1;
        QString aerogestPassword = QInputDialog::getText(nullptr, QObject::tr("Aerogest password"), QObject::tr("Password of the aerogest database"), QLineEdit::Password);
        if (aerogestPassword.isEmpty())
            return -1;
        settings.beginGroup("aerogest");
        settings.setValue("password", aerogestPassword);
        settings.setValue("path", aerogestPath);
        settings.endGroup();
        settings.sync();
    }

    // Try to connect to Access file...
    QSqlDatabase aerogest = QSqlDatabase::addDatabase("QODBC", "aerogest");
    aerogest.setHostName("localhost");
    aerogest.setDatabaseName("DRIVER={Microsoft Access Driver (*.mdb)};FIL={MS Access};DBQ=" + settings.value("aerogest/path").toString());
    aerogest.setPassword(settings.value("aerogest/password").toString());
    if (!aerogest.open()) {
        QMessageBox::warning(nullptr, QObject::tr("Failed to connect to aerogest"), QObject::tr("Unable to connect to aerogest : %1").arg(aerogest.lastError().driverText()));
        return -1;
    }

    LoginDialog loginDialog;
    if (!loginDialog.exec()) {
        // User cancelled
        return 0;
    }

    // Check login from loginDialog.login() loginDialog.password()
    QSqlQuery loginQuery(aerogest);
    loginQuery.prepare("SELECT NiveauAutorisation FROM Adherents_administratif WHERE Login = ? AND Password = ?");
    loginQuery.bindValue(0, loginDialog.login());
    loginQuery.bindValue(1, loginDialog.password());
    if (!loginQuery.exec()) {
        QMessageBox::warning(nullptr, QObject::tr("Database error"), QObject::tr("Error while searching aerogest database : %1").arg(loginQuery.lastError().driverText()));
        return -1;
    }
    if (!loginQuery.next()) {
        QMessageBox::warning(nullptr, QObject::tr("Invalid login"), QObject::tr("Unknown user/password"));
        return -1;
    }
    if (loginQuery.value(0).toInt() < 45) {
        QMessageBox::warning(nullptr, QObject::tr("Invalid login"), QObject::tr("Your user is not allowed"));
        return -1;
    }

    MainWindow w;
    w.show();

    return a.exec();
}
