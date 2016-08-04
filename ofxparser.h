#ifndef OFXPARSER_H
#define OFXPARSER_H

#include <QObject>
#include <QIODevice>
#include <QDate>
#include <vector>

struct StatementLine {
    QDate operationDate;
    int amountInCents;
    QString memo;
    QString type;
    QString idFromBank;
    QString name;
    bool matched;
};

struct BankAccount {
    QString iban;
    std::vector<StatementLine> statement;
};

class OfxParser : public QObject
{
    Q_OBJECT
public:
    explicit OfxParser(QIODevice *source, QObject *parent = 0);

    std::vector<BankAccount> parse();
private:
    void parsedData(std::vector<BankAccount> &targetResult, const QString &path, const QMap<QString, QString> &data);
    QIODevice *m_file;
};

#endif // OFXPARSER_H
