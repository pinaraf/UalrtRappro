#include "ofxparser.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>
#include <cmath>

OfxParser::OfxParser(QIODevice *source, QObject *parent)
    : QObject(parent), m_file(source)
{

}

std::vector<BankAccount> OfxParser::parse()
{
    std::vector<BankAccount> result;
    QTextStream stream(m_file);
    QString line;
    QRegularExpression newTagMatcher("^<(\\w+)>$");
    QRegularExpression endTagMatcher("^</(\\w+)>$");
    QRegularExpression dataTagMatcher("^<(\\w+)>(.*)$");

    QStringList tagList;
    QMap<QString, QString> dataMap;
    while (stream.readLineInto(&line)) {
        if (line.isEmpty())
            continue;
        auto match = newTagMatcher.match(line);
        if (match.hasMatch()) {
            if (!dataMap.isEmpty()) {
                parsedData(result, tagList.join('/'), dataMap);
                dataMap.clear();
            }
            tagList.append(match.captured(1));
            continue;
        }
        match = endTagMatcher.match(line);
        if (match.hasMatch()) {
            if (!dataMap.isEmpty()) {
                parsedData(result, tagList.join('/'), dataMap);
                dataMap.clear();
            }
            tagList.pop_back();
            continue;
        }
        match = dataTagMatcher.match(line);
        if (match.hasMatch()) {
            dataMap.insert(match.captured(1), match.captured(2));
            continue;
        }
        qFatal("Invalid content");
    }
    return result;
}

qint64 convertFrenchBBAN(const QString &number) {
    qint64 result = 0;
    for (const QChar &character: number) {
        if (character.isDigit()) {
            result = result * 10 + (character.toLatin1() - '0');
        } else if (character.isLetter()) {
            result = result * 10 + (((character.toUpper().toLatin1() - 'A') % 9) + 1);
        }
    }
    return result;
}

void OfxParser::parsedData(std::vector<BankAccount> &targetResults, const QString &path, const QMap<QString, QString> &data)
{
    //qDebug() << "DATA@" << path << " : " << data;
    if (path == "OFX/BANKMSGSRSV1/STMTTRNRS/STMTRS/BANKACCTFROM") {
        int ribKey = 97 - ((convertFrenchBBAN(data["BANKID"]) * 89 + convertFrenchBBAN(data["BRANCHID"]) * 15 + convertFrenchBBAN(data["ACCTID"]) * 3) % 97);
        QString bban = (data["BANKID"] + data["BRANCHID"] + data["ACCTID"] + QString::number(ribKey)).toUpper();
        QString tempIban = bban + "FR00";
        // Calculate the iban key...
        for (char i = 'A' ; i <= 'Z' ; i++) {
            tempIban.replace(i, QString::number(i - 'A' + 10));
        }
        int ibanKey = 0;
        int n = tempIban.left(9).toInt() % 97;
        tempIban = tempIban.mid(9);
        while (tempIban.length() > 0) {
            n = (QString::number(n) + tempIban.left(7)).toInt() % 97;
            tempIban = tempIban.mid(7);
        }
        ibanKey = 98 - n % 97;
        QString iban = QString("FR%1%2").arg(ibanKey, 2, 10, QLatin1Char('0')).arg(bban);
        BankAccount newAccount;
        newAccount.iban = iban;
        targetResults.push_back(newAccount);
    } else if (path == "OFX/BANKMSGSRSV1/STMTTRNRS/STMTRS/BANKTRANLIST/STMTTRN") {
        StatementLine line;
        line.operationDate = QDate::fromString(data["DTPOSTED"], "yyyyMMdd");
        line.amountInCents = ::round(data["TRNAMT"].toDouble() * 100);
        line.memo = data["MEMO"];
        line.type = data["TRNTYPE"];
        line.idFromBank = data["FITID"];
        line.name = data["NAME"];
        line.matched = false;
        targetResults.back().statement.push_back(line);
    }
}
