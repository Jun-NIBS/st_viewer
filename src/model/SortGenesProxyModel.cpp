#include "SortGenesProxyModel.h"

#include <QString>
#include <QDebug>

namespace
{

SortGenesProxyModel::SortCategory sortCategory(const QString &name)
{
    if (name.size() == 0) {
        return SortGenesProxyModel::numericGene;
    }

    if (!name[0].isLetter()) {
        return SortGenesProxyModel::numericGene;
    }

    if (name.startsWith("ambiguous")) {
        return SortGenesProxyModel::ambiguousGene;
    }

    return SortGenesProxyModel::normalGene;
}

bool geneNameLessThan(const QString &geneName1,
                      const QString &geneName2,
                      const Qt::CaseSensitivity &caseSensitivity,
                      bool isSortLocaleAware)
{
    SortGenesProxyModel::SortCategory geneName1_category = sortCategory(geneName1);
    SortGenesProxyModel::SortCategory geneName2_category = sortCategory(geneName2);

    if (geneName1_category == geneName2_category) {
        if (isSortLocaleAware) {
            return geneName1.localeAwareCompare(geneName2) < 0;
        } else {
            return geneName1.compare(geneName2, caseSensitivity) < 0;
        }
    }

    return geneName2_category < geneName1_category;
}
}

SortGenesProxyModel::SortGenesProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

SortGenesProxyModel::~SortGenesProxyModel()
{
}

bool SortGenesProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QAbstractTableModel *model = qobject_cast<QAbstractTableModel *>(sourceModel());
    Q_ASSERT(model);

    QString leftName;
    QString rightName;
    bool leftNameFound = false;
    bool rightNameFound = false;
    // As this proxy sorter is used in wrapper for models, the models
    // must have a method called geneName in order for the sorting to work
    QMetaObject::invokeMethod(model,
                              "geneName",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(bool, leftNameFound),
                              Q_ARG(const QModelIndex &, left),
                              Q_ARG(QString &, leftName));
    QMetaObject::invokeMethod(model,
                              "geneName",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(bool, rightNameFound),
                              Q_ARG(const QModelIndex &, right),
                              Q_ARG(QString &, rightName));

    if (leftNameFound && rightNameFound) {
        return geneNameLessThan(leftName, rightName, sortCaseSensitivity(), isSortLocaleAware());
    }

    return QSortFilterProxyModel::lessThan(left, right);
}
