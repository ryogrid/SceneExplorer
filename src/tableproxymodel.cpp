#include <QtDebug>

#include "tablemodel.h"
#include "tableproxymodel.h"

bool FileMissingFilterProxyModel::filterAcceptsRow(
        int sourceRow,
        const QModelIndex &sourceParent) const
{
    if ( (static_cast<TableModel*>(sourceModel()))->IsShowMissing() )
        return true;

    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    QVariant v = sourceModel()->data(index, TableModel::MovieFileFull);
    QString s = v.toString();
    return QFile(s).exists();
}
//void FileMissingFilterProxyModel::ensureBottom()
//{
//    int allcount = rowCount();
//    int startProxyI = allcount-(3*10);
//    if (startProxyI < 0)
//        return;

//    for(int i=startProxyI; i < allcount; ++i)
//    {
//        for(int col = 0 ; col < 5; ++col)
//        {
//            QModelIndex sourceIndex = mapToSource(index(i,col));
//            sourceModel()->data(sourceIndex,Qt::DecorationRole);
//            sourceModel()->data(sourceIndex,Qt::DisplayRole);
//        }
////        sourceModel()->data(sourceModel()->index(i,1),Qt::DecorationRole);
////        sourceModel()->data(sourceModel()->index(i,2),Qt::DecorationRole);
////        sourceModel()->data(sourceModel()->index(i,3),Qt::DecorationRole);
////        sourceModel()->data(sourceModel()->index(i,4),Qt::DecorationRole);
//    }
//}
void FileMissingFilterProxyModel::ensureIndex(const QModelIndex& mi)
{
    int startI = mi.row();
    startI = startI -(3*10);
    if(startI < 0)
        startI=0;

    int endI = mi.row() + 10;
    if(endI >= rowCount())
        endI = rowCount();

    int startSourceI = mapToSource(index(startI,0)).row();
    int endSourceI = mapToSource(index(endI,0)).row();
    for(int i=startSourceI; i < endSourceI; ++i)
    {
        sourceModel()->data(sourceModel()->index(i,0),Qt::DecorationRole);
        sourceModel()->data(sourceModel()->index(i,1),Qt::DecorationRole);
        sourceModel()->data(sourceModel()->index(i,2),Qt::DecorationRole);
        sourceModel()->data(sourceModel()->index(i,3),Qt::DecorationRole);
        sourceModel()->data(sourceModel()->index(i,4),Qt::DecorationRole);
    }
}
QModelIndex FileMissingFilterProxyModel::findIndex(const QString& selPath, const FIND_INDEX& fi) const
{
    QModelIndex sourceIndex = (static_cast<TableModel*>(sourceModel()))->GetIndex(selPath);

    if(!sourceIndex.isValid())
    {
        qDebug() << "sourceIndex is Invalid" << __FUNCTION__;
        return QModelIndex();
    }

    int toAdd=0;
    if(fi==FIND_INDEX::FIND_INDEX_TITLE)
        toAdd=1;
    else if(fi==FIND_INDEX::FIND_INDEX_DESCRIPTION)
        toAdd=2;

    QModelIndex retSourceIndex = (static_cast<TableModel*>(sourceModel()))->index(sourceIndex.row() + toAdd, sourceIndex.column());
    QModelIndex retIndex = mapFromSource(retSourceIndex);
    qDebug() << "source:" << retSourceIndex << " after:" << retIndex << __FUNCTION__;
    return retIndex;
}
