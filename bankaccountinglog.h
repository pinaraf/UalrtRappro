#ifndef BANKACCOUNTINGLOG_H
#define BANKACCOUNTINGLOG_H

#include <QDialog>

namespace Ui {
class BankAccountingLog;
}

class BankAccountingLog : public QDialog
{
    Q_OBJECT

public:
    explicit BankAccountingLog(QWidget *parent = 0);
    ~BankAccountingLog();

    void setIban(const QString &iban);

    QString accountName() const;
    QString accountId() const;
private slots:
    void on_accountName_textChanged(const QString &arg1);

    void on_accountingLogsView_activated(const QModelIndex &index);

private:
    Ui::BankAccountingLog *ui;
};

#endif // BANKACCOUNTINGLOG_H
