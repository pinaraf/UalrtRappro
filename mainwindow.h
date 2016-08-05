#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStyledItemDelegate>
#include "statementtablemodel.h"

class QSqlTableModel;

namespace Ui {
class MainWindow;
}


class MoneyInCentsItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit MoneyInCentsItemDelegate (QObject *parent = Q_NULLPTR);
    virtual QString displayText(const QVariant &value, const QLocale &locale) const;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionImport_file_triggered();
    void on_actionQuit_triggered();
    void on_bankAccountList_currentIndexChanged(int index);

    void refreshFromSelection();
    void refreshData();

    void on_actionMatch_triggered();

    void on_actionAutomatic_match_triggered();

private:
    Ui::MainWindow *ui;
    StatementTableModel *statementTableModel;
};

#endif // MAINWINDOW_H
