#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ofxparser.h"

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
            QString baseName = settingsFolder.dirName();
            settingsFolder.cdUp();
            settingsFolder.mkdir(baseName);
            settingsFolder.cd(baseName);
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
                //QInput
                QString bankAccountName = QInputDialog::getText(this, tr("Bank account name"), tr("New bank account found.\nPlease give a name for the account with IBAN %1").arg(acct.iban));
                if (bankAccountName.isEmpty())
                    return;
                accountQuery.prepare("INSERT INTO bank_account (currency, iban, name) VALUES ('EUR', :iban, :name);");
                accountQuery.bindValue(":iban", acct.iban);
                accountQuery.bindValue(":name", bankAccountName);
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
}

void MainWindow::on_bankAccountList_currentIndexChanged(int index)
{
    statementTableModel->setFilter(QString("account=%1").arg(ui->bankAccountList->itemData(index).toLongLong()));
    statementTableModel->setSort(2, Qt::DescendingOrder);
    statementTableModel->select();
    ui->statementTable->resizeColumnsToContents();
    ui->actionMatch->setEnabled(false);
}

void MainWindow::refreshFromSelection()
{
    ui->actionMatch->setEnabled(ui->statementTable->selectionModel()->selectedIndexes().size() > 0);
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
