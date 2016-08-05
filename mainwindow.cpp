#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ofxparser.h"
#include "automatchresultdialog.h"
#include "bankaccountinglog.h"

#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QInputDialog>
#include <QSqlError>
#include <QSqlTableModel>
#include <QStyledItemDelegate>
#include <QPainter>

MoneyInCentsItemDelegate::MoneyInCentsItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

QString MoneyInCentsItemDelegate::displayText(const QVariant &value, const QLocale &locale) const
{
    return locale.toCurrencyString(value.toLongLong() / 100., "€", 2);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->statementTable, SIGNAL(activated(QModelIndex)), this, SLOT(refreshFromSelection()));
    connect(ui->statementTable, SIGNAL(entered(QModelIndex)), this, SLOT(refreshFromSelection()));
    connect(ui->statementTable, SIGNAL(clicked(QModelIndex)), this, SLOT(refreshFromSelection()));

    QString sqlitePath = QStandardPaths::locate(QStandardPaths::AppDataLocation, "statement.sqlite");
    if (sqlitePath.isEmpty()) {
        // No database yet...
        QDir settingsFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (!settingsFolder.exists()) {
            settingsFolder.mkpath(".");
        }
        sqlitePath = settingsFolder.absoluteFilePath("statement.sqlite");

        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "statement");
        db.setDatabaseName(sqlitePath);
        if (!db.open()) {
            QMessageBox::warning(this, tr("Error creating database"), tr("Could not open the statement database. Please contact support."));
            qApp->exit(-1);
            return;
        }

        // Now create the schema by reading :/schema.sql
        QFile schemaFile(":/schema.sql");
        if (!schemaFile.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, tr("Error creating database"), tr("Could not read the schema. Please contact support."));
            qApp->exit(-1);
            return;
        }
        QString schemaContent = QString::fromUtf8(schemaFile.readAll());
        for (QString &line: schemaContent.split('\n')) {
            QSqlQuery q = db.exec(line);
            if (q.lastError().isValid()) {
                QMessageBox::warning(this, tr("Error creating database"), tr("Could not create the statement database. Please contact support.\nDetails : %1").arg(q.lastError().databaseText()));
                qApp->exit(-1);
                return;
            }
        }
    } else {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "statement");
        db.setDatabaseName(sqlitePath);
        if (!db.open()) {
            QMessageBox::warning(this, tr("Error creating database"), tr("Could not open the statement database. Please contact support."));
            qApp->exit(-1);
            return;
        }
    }
    statementTableModel = new StatementTableModel(this, QSqlDatabase::database("statement"));
    statementTableModel->setTable("statement_line");
    statementTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    ui->statementTable->setModel(statementTableModel);
    ui->statementTable->setColumnHidden(0, true);
    ui->statementTable->setColumnHidden(1, true);
    ui->statementTable->setColumnHidden(5, true);
    ui->statementTable->setColumnHidden(6, true);
    ui->statementTable->setColumnHidden(8, true);
    ui->statementTable->setColumnHidden(9, true);
    ui->statementTable->setItemDelegateForColumn(3, new MoneyInCentsItemDelegate(this));
    refreshData();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionQuit_triggered()
{
    qApp->exit();
}

void MainWindow::on_actionImport_file_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import a bank file"), "", tr("OFX file (*.ofx)"));
    if (!fileName.isNull()) {
        QFile data(fileName);
        if (!data.open(QFile::ReadOnly)) {
            QMessageBox::warning(this, tr("Failed to open file"), tr("The bank file could not be opened"));
            return;
        }
        OfxParser parser(&data);
        std::vector<BankAccount> accounts = parser.parse();
        int statementLines = 0;
        int importedLines = 0;
        int alreadyImportedLines = 0;
        QSqlDatabase db = QSqlDatabase::database("statement");
        if (!db.transaction()) {
            QMessageBox::warning(this, tr("Failed to prepare database"), tr("Database error while preparing for insertion : %1").arg(db.lastError().databaseText()));
            return;
        }

        for (BankAccount &acct: accounts) {
            statementLines += acct.statement.size();

            // Find out the account in the statement db
            QSqlQuery accountQuery(db);
            accountQuery.prepare("SELECT id FROM bank_account WHERE iban = :iban;");
            accountQuery.bindValue(":iban", acct.iban);
            if (!accountQuery.exec()) {
                QMessageBox::warning(this, tr("Failed to query database"), tr("Database error : %1").arg(accountQuery.lastError().databaseText()));
                return;
            }
            int accountId = -1;
            if (!accountQuery.next()) {
                qDebug() << "Unknown iban " << acct.iban;

                BankAccountingLog bankLogDialog(this);
                bankLogDialog.setIban(acct.iban);
                if (!bankLogDialog.exec())
                    return;

                accountQuery.prepare("INSERT INTO bank_account (currency, iban, name, accounting_id) VALUES ('EUR', :iban, :name, :accounting);");
                accountQuery.bindValue(":iban", acct.iban);
                accountQuery.bindValue(":name", bankLogDialog.accountName());
                accountQuery.bindValue(":accounting", bankLogDialog.accountId());
                if (!accountQuery.exec()) {
                    QMessageBox::warning(this, tr("Failed to insert account"), tr("Database error : %1").arg(accountQuery.lastError().databaseText()));
                    return;
                }
                accountId = accountQuery.lastInsertId().toInt();
            } else {
                accountId = accountQuery.value(0).toInt();
            }
            qDebug() << accountId;
            QSqlQuery checkStatementQuery(db);
            QSqlQuery insertStatementQuery(db);
            checkStatementQuery.prepare("SELECT id FROM statement_line WHERE account = :account AND bank_id = :bank_id");
            insertStatementQuery.prepare("INSERT INTO statement_line (account, effective_date, amount_in_cents, memo, line_type, bank_id, name) "
                                         "VALUES (:account, :date, :amount, :memo, :type, :bank_id, :name);");
            for (StatementLine &stmt: acct.statement) {
                checkStatementQuery.bindValue(":account", accountId);
                checkStatementQuery.bindValue(":bank_id", stmt.idFromBank);
                checkStatementQuery.exec();
                if (checkStatementQuery.next()) {
                    // Statement already imported...
                    alreadyImportedLines++;
                } else {
                    insertStatementQuery.bindValue(":account", accountId);
                    insertStatementQuery.bindValue(":date", stmt.operationDate);
                    insertStatementQuery.bindValue(":amount", stmt.amountInCents);
                    insertStatementQuery.bindValue(":memo", stmt.memo);
                    insertStatementQuery.bindValue(":type", stmt.type);
                    insertStatementQuery.bindValue(":bank_id", stmt.idFromBank);
                    insertStatementQuery.bindValue(":name", stmt.name);
                    if (!insertStatementQuery.exec()) {
                        QMessageBox::warning(this, tr("Failed to insert line"), tr("Database error : %1 (%2)")
                                             .arg(insertStatementQuery.lastError().databaseText())
                                             .arg(insertStatementQuery.lastError().driverText()));
                        return;
                    }
                    importedLines++;
                }
            }
        }

        if (!db.commit()) {
            QMessageBox::warning(this, tr("Failed to commit database"), tr("Database error while commiting : %1").arg(db.lastError().databaseText()));
            return;
        }

        QMessageBox::information(this, tr("Parse result"), tr("Parsed %1 accounts from this file, for %2 statement lines\nImported %3 lines (%4 already found)")
                                 .arg(accounts.size())
                                 .arg(statementLines)
                                 .arg(importedLines)
                                 .arg(alreadyImportedLines));
        refreshData();
    }
}

void MainWindow::refreshData()
{
    QSqlQuery accountList("SELECT id, name, iban FROM bank_account ORDER BY id;", QSqlDatabase::database("statement"));
    accountList.exec();
    ui->bankAccountList->clear();
    while (accountList.next()) {
        ui->bankAccountList->addItem(QString("%1 (%2)").arg(accountList.value(1).toString()).arg(accountList.value(2).toString()), accountList.value(0));
    }
    ui->actionMatch->setEnabled(false);
    ui->actionAutomatic_match->setEnabled(false);
}

void MainWindow::on_bankAccountList_currentIndexChanged(int index)
{
    statementTableModel->setFilter(QString("account=%1").arg(ui->bankAccountList->itemData(index).toLongLong()));
    statementTableModel->setSort(2, Qt::DescendingOrder);
    statementTableModel->select();
    ui->statementTable->resizeColumnsToContents();
    ui->actionMatch->setEnabled(false);
    ui->actionAutomatic_match->setEnabled(false);
}

void MainWindow::refreshFromSelection()
{
    ui->actionMatch->setEnabled(ui->statementTable->selectionModel()->selectedIndexes().size() > 0);
    ui->actionAutomatic_match->setEnabled(ui->statementTable->selectionModel()->selectedIndexes().size() > 0);
}

void MainWindow::on_actionMatch_triggered()
{
    qDebug() << "Mark as matched, go go go !";
    QModelIndexList selectedRows = ui->statementTable->selectionModel()->selectedRows();
    if (selectedRows.size() == 0) {
        QMessageBox::warning(this, tr("Missing selection"), tr("Please select statement lines to match first."));
        return;
    }
    for (QModelIndex &idx : selectedRows) {
        qDebug() << statementTableModel->setData(idx.sibling(idx.row(), 8), true);
    }
    statementTableModel->submitAll();
}

void MainWindow::on_actionAutomatic_match_triggered()
{
    qDebug() << "Mark as matched, go go go !";
    QModelIndexList selectedRows = ui->statementTable->selectionModel()->selectedRows();
    if (selectedRows.size() == 0) {
        QMessageBox::warning(this, tr("Missing selection"), tr("Please select statement lines to match first."));
        return;
    }

    // Fetch the accounting id for the selected bank account
    //statementTableModel->setFilter(QString("account=%1").arg(ui->bankAccountList->itemData(index).toLongLong()));
    int selectedBankAccount = ui->bankAccountList->itemData(ui->bankAccountList->currentIndex()).toLongLong();
    QSqlQuery bankAccountInfo(QSqlDatabase::database("statement"));
    bankAccountInfo.prepare("SELECT accounting_id FROM bank_account WHERE id = ? AND accounting_id IS NOT NULL AND accounting_id <> ''; ");
    bankAccountInfo.bindValue(0, selectedBankAccount);
    if (!bankAccountInfo.exec() || !bankAccountInfo.next()) {
        QMessageBox::warning(this, tr("Invalid bank account"), tr("No automatic match possible with this bank account, it has no corresponding accounting log."));
        return;
    }
    QString accountingId = bankAccountInfo.value(0).toString();
    qDebug() << accountingId;
    if (true)
        return;

    QSqlQuery aerogestSearchDebit(QSqlDatabase::database("aerogest"));
    aerogestSearchDebit.prepare("SELECT Id_enr, Libelle, Debit, Credit, MA_date, commentaire FROM Compta_EcrituresDetail WHERE Numcompte = ? AND Debit = ? AND NOT(pointageVerouille);");
    QSqlQuery aerogestSearchCredit(QSqlDatabase::database("aerogest"));
    aerogestSearchCredit.prepare("SELECT Id_enr, Libelle, Debit, Credit, MA_date, commentaire FROM Compta_EcrituresDetail WHERE Numcompte = ? AND Credit = ? AND NOT(pointageVerouille);");
    QSqlQuery aerogestMarkMatch(QSqlDatabase::database("aerogest"));
    aerogestMarkMatch.prepare("UPDATE Compta_EcrituresDetail SET pointageVerouille=True, Pointage=? WHERE Id_enr = ?;");

    for (QModelIndex &idx : selectedRows) {
        // If line is already marked as matched, skip it
        if (statementTableModel->data(idx.sibling(idx.row(), 8)).toBool())
            continue;

        QDate lineDate = statementTableModel->data(idx.sibling(idx.row(), 2)).toDate();
        QString lineLabel = statementTableModel->data(idx.sibling(idx.row(), 7)).toString();
        QString lineType = statementTableModel->data(idx.sibling(idx.row(), 4)).toString();
        int amountInCents = statementTableModel->data(idx.sibling(idx.row(), 3)).toInt();
        qDebug() << "Searching for ..." << amountInCents << " euros cts";
        QSqlQuery &targetQuery = aerogestSearchDebit;
        if (amountInCents < 0) {
            targetQuery = aerogestSearchCredit;
            amountInCents = -amountInCents;
        }
        targetQuery.bindValue(0, accountingId);
        targetQuery.bindValue(1, amountInCents / 100.);
        if (!targetQuery.exec()) {
            qDebug() << "Failed to execute query : " << targetQuery.lastError().databaseText();
            QMessageBox::warning(this, tr("Database error"), tr("Failed to search the Aerogest base : %1").arg(targetQuery.lastError().databaseText()));
            break;
        }
        if (targetQuery.numRowsAffected() == 0) {
            qDebug() << "No result, continue...";
            continue;
        }

        AutoMatchResultDialog dialog(this);
        dialog.setStatementInformation(lineDate, amountInCents, lineType, lineLabel);
        dialog.setMatchQuery(targetQuery);
        if (dialog.exec()) {
            // Mark the line as matched in our base, and in aerogest...

            qDebug() << dialog.selectedId();
            aerogestMarkMatch.bindValue(0, lineDate);
            aerogestMarkMatch.bindValue(1, dialog.selectedId());
            if (!aerogestMarkMatch.exec()) {
                QMessageBox::warning(this, tr("Database error"), tr("Failed to update the Aerogest base : %1").arg(aerogestMarkMatch.lastError().databaseText()));
                break;
            }
            qDebug() << statementTableModel->setData(idx.sibling(idx.row(), 8), true);
            qDebug() << statementTableModel->setData(idx.sibling(idx.row(), 9), dialog.selectedId());
        } else {
            // User rejected that automatic match, too bad
        }
    }
    statementTableModel->submitAll();
}
