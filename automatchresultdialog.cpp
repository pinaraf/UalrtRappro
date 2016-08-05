#include "automatchresultdialog.h"
#include "ui_automatchresultdialog.h"
#include <QDate>
#include <QSqlQueryModel>

AutoMatchResultDialog::AutoMatchResultDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AutoMatchResultDialog),
    resultsModel(new QSqlQueryModel(this))
{
    ui->setupUi(this);
    ui->matchResultView->setModel(resultsModel);
}

AutoMatchResultDialog::~AutoMatchResultDialog()
{
    delete ui;
}

void AutoMatchResultDialog::setStatementInformation(const QDate &statementDate, int amountInCents, const QString &type, const QString &label)
{
    ui->statementDate->setText(statementDate.toString(Qt::SystemLocaleLongDate));
    ui->statementAmount->setText(QLocale::system().toCurrencyString(amountInCents / 100., "€"));
    ui->statementText->setText(label);
    ui->statementType->setText(type);
}

void AutoMatchResultDialog::setMatchQuery(const QSqlQuery &query)
{
    resultsModel->setQuery(query);
    if (resultsModel->rowCount() == 1) {
        ui->matchButton->setEnabled(true);
        ui->matchButton->setFocus();
        ui->matchResultView->selectRow(0);
    } else {
        ui->matchButton->setEnabled(false);
    }
    ui->matchResultView->setColumnHidden(0, true);
    ui->matchResultView->resizeColumnsToContents();
}

int AutoMatchResultDialog::selectedId() const
{
    QModelIndexList selectedRows = ui->matchResultView->selectionModel()->selectedRows();
    if (selectedRows.size() == 0) {
        return -1;
    }
    QModelIndex idx = selectedRows.first();
    return resultsModel->data(idx.sibling(idx.row(), 0)).toInt();
}

void AutoMatchResultDialog::on_matchResultView_clicked(const QModelIndex &index)
{
    if (ui->matchResultView->selectionModel()->selectedRows().size() == 1)
        ui->matchButton->setEnabled(true);
    else
        ui->matchButton->setEnabled(false);
}
