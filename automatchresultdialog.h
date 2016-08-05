#ifndef AUTOMATCHRESULTDIALOG_H
#define AUTOMATCHRESULTDIALOG_H

#include <QDialog>

class QSqlQuery;
class QSqlQueryModel;
class QDate;

namespace Ui {
class AutoMatchResultDialog;
}

class AutoMatchResultDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AutoMatchResultDialog(QWidget *parent = 0);
    ~AutoMatchResultDialog();

    void setStatementInformation(const QDate &statementDate, int amountInCents, const QString &type, const QString &label);
    void setMatchQuery(const QSqlQuery &query);
    int selectedId() const;
private slots:
    void on_matchResultView_clicked(const QModelIndex &index);

private:
    Ui::AutoMatchResultDialog *ui;
    QSqlQueryModel *resultsModel;
};

#endif // AUTOMATCHRESULTDIALOG_H
