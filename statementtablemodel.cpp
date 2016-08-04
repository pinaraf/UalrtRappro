#include "statementtablemodel.h"
#include <QDebug>
#include <QBrush>

StatementTableModel::StatementTableModel(QObject *parent, QSqlDatabase db)
    : QSqlTableModel(parent, db)
{

}

QVariant StatementTableModel::data(const QModelIndex &index, int role) const
{
    // A few special cases...
    if (role == Qt::BackgroundRole && index.column() == 2)
    {
        if (this->data(index.sibling(index.row(), 8), Qt::DisplayRole).toBool())
            return QBrush(Qt::green);
        else
            return QBrush(Qt::red);
    }
    // And the amount too
    if (role == Qt::ForegroundRole && index.column() == 3)
    {
        if (this->data(index, Qt::DisplayRole).toLongLong() < 0)
            return QBrush(Qt::red);
        else
            return QBrush(Qt::green);
    }

    return QSqlTableModel::data(index, role);
}

//virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
