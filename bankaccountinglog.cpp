#include "bankaccountinglog.h"
#include "ui_bankaccountinglog.h"
#include <QSqlQueryModel>
#include <QPushButton>

BankAccountingLog::BankAccountingLog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BankAccountingLog)
{
    ui->setupUi(this);
    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery("SELECT [nom du journal], Contrepartie FROM Compta_Journaux WHERE TypeJournal = 'BQ' and Contrepartie <> '';", QSqlDatabase::database("aerogest"));
    ui->accountingLogsView->setModel(model);
    ui->accountingLogsView->resizeColumnsToContents();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

BankAccountingLog::~BankAccountingLog()
{
    delete ui;
}

void BankAccountingLog::setIban(const QString &iban)
{
    ui->label->setText(ui->label->text().arg(iban));
}

void BankAccountingLog::on_accountName_textChanged(const QString &arg1)
{
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!arg1.isEmpty());
}

void BankAccountingLog::on_accountingLogsView_activated(const QModelIndex &index)
{
    Q_UNUSED(index);
    QModelIndexList selectedRows = ui->accountingLogsView->selectionModel()->selectedRows();
    if (selectedRows.size() == 1) {
        QModelIndex idx = selectedRows.first();
        ui->accountName->setText(ui->accountingLogsView->model()->data(idx.sibling(idx.row(), 0)).toString());
    }
}

QString BankAccountingLog::accountName() const
{
    return ui->accountName->text();
}

QString BankAccountingLog::accountId() const
{
    QModelIndexList selectedRows = ui->accountingLogsView->selectionModel()->selectedRows();
    if (selectedRows.size() == 1) {
        QModelIndex idx = selectedRows.first();
        return ui->accountingLogsView->model()->data(idx.sibling(idx.row(), 1)).toString();
    }
    return QString::null;
}
