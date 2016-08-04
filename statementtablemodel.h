#ifndef STATEMENTTABLEMODEL_H
#define STATEMENTTABLEMODEL_H

#include <QSqlTableModel>

class StatementTableModel : public QSqlTableModel
{
    Q_OBJECT
public:
    StatementTableModel(QObject *parent = Q_NULLPTR, QSqlDatabase db = QSqlDatabase());
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

#endif // STATEMENTTABLEMODEL_H
