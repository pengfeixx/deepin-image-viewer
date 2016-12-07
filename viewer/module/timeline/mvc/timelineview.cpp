#include "application.h"
#include "timelineview.h"
#include "timelineitem.h"
#include "timelinemodel.h"
#include "controller/dbmanager.h"
#include "utils/baseutils.h"
#include "utils/imageutils.h"
#include "widgets/scrollbar.h"
#include <QDebug>
#include <QFileInfo>
#include <QHeaderView>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>
#include <QtConcurrent>
#include <QtMath>

namespace {

const QColor BACKGROUND_COLOR = QColor("#1B1B1B");
const QColor BORDER_COLOR_SELECTED = QColor("#01bdff");

QVariant genThumbnail(const QVariant &data)
{
    using namespace utils::image;

    QString path = data.toList().first().toString();
    QModelIndex index = data.toList().last().value<QModelIndex>();

    QVariant result;
    const QPixmap thumb = getThumbnail(path);
    if (thumb.isNull()) {
        // Can't generate thumbnail, remove it from database
        dApp->dbM->removeImgInfos(QStringList(path));
    }
    else {
        QByteArray inByteArray;
        QBuffer inBuffer( &inByteArray );
        inBuffer.open( QIODevice::WriteOnly );
        // write inPixmap into inByteArray
        if ( ! cutSquareImage(thumb).save( &inBuffer, "JPG" )) {
            // qDebug() << "Write pixmap to buffer error!" << info.name;
        }
        QVariantList nl;
        nl << QVariant(index) << QVariant(inByteArray);
        result = QVariant(nl);
    }

    return result;
}

}
TimelineView::TimelineView(QWidget *parent)
    : QAbstractItemView(parent)
    , m_itemSize(96)
    , m_titleHeight(30)
    , m_hItemSpacing(4)
    , m_vItemSpacing(4)
    , m_bottomMargin(30)
    , m_topMargin(44)
{
    setVerticalScrollBar(new ScrollBar());
    verticalScrollBar()->setContextMenuPolicy(Qt::PreventContextMenu);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &TimelineView::onScrolled);

    connect(&m_watcher, SIGNAL(resultReadyAt(int)),
            this, SLOT(onThumbnailGenerated(int)));
}

void TimelineView::setItemSize(int size)
{
    if (size <=0) {
        return;
    }
    m_itemSize = size;
    updateVisualRects();
    viewport()->update();
}

void TimelineView::setTitleHeight(int height)
{
    m_titleHeight = height;
    updateVisualRects();
    viewport()->update();
}

void TimelineView::updateView()
{
    updateVerticalScrollbar();
}

QSize TimelineView::viewportSizeHint() const
{
    int titleRowCount = model()->rowCount();
    int viewportHeight = 0;
    for (int trc = 0; trc < titleRowCount; trc ++) {
        QModelIndex titleIndex = model()->index(trc, 0);
        // Get height of data-row
        int colmunCount = model()->columnCount(titleIndex);
        int subRowCount = qCeil(1.0 * colmunCount / maxColumnCount());

        viewportHeight += m_titleHeight + m_vItemSpacing
                + subRowCount * (m_itemSize + m_vItemSpacing);
    }

    return QSize(this->width(), viewportHeight + m_topMargin + m_bottomMargin);
}

QModelIndex TimelineView::indexAt(const QPoint &point) const
{
    for (auto i : m_irList) {
        if (visualRect(i.index).contains(point)) {
            return i.index;
        }
    }

    return QModelIndex();
}

void TimelineView::scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint hint)
{
    if (index.parent() != rootIndex() /*|| index.column() != d->column*/)
        return;

    const QRect rect = visualRect(index);
    if (hint == EnsureVisible && viewport()->rect().contains(rect)) {
        viewport()->update(rect);
        return;
    }

//    verticalScrollBar()->setValue(d->verticalScrollToValue(index, rect, hint));
}

QRect TimelineView::visualRect(const QModelIndex &index) const
{
    QRect r = m_irMap.value(index);
    r.setY(r.y() - verticalOffset());
    r.setHeight(r.height() - verticalOffset());
    return r;

}

int TimelineView::horizontalOffset() const
{
    const int contentWidth = (m_itemSize + m_hItemSpacing) * maxColumnCount();
    return (this->width() - contentWidth) / 2;
}

bool TimelineView::isIndexHidden(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return false;
}

QModelIndex TimelineView::moveCursor(QAbstractItemView::CursorAction cursorAction,
                                 Qt::KeyboardModifiers modifiers)
{
    // the child views which have focus get to deal with this first and if
    // they don't accept it then it comes up this view and we only grip left/right
    Q_UNUSED(modifiers);
    if (! model())
        return QModelIndex();

    auto indexs = selectionModel()->selectedIndexes();
    if (indexs.length() != 1)
        return QModelIndex();

//    QModelIndex current = currentIndex();
//    switch (cursorAction) {
//    case MoveLeft:
//        if (current.parent().isValid() && current.parent() != rootIndex())
//            return (current.parent());
//        else
//            return current;

//    case MoveRight:
//        if (model()->hasChildren(current))
//            return model()->index(0, 0, current);
//        else
//            return current.sibling(current.row() + 1, current.column());

//    case MoveNext:
//        if (model()->hasChildren(current)) {
//            return model()->index(0, 0, current);
//        }
//        else {
//            return model()->index(0, 0);
//        }
//    default:
//        break;
//    }

    return QModelIndex();
}

void TimelineView::setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags flags)
{
    QRect vr;
    if (flags & QItemSelectionModel::Current) {
        int x = rect.width() < 0 ? rect.x() + rect.width() : rect.x();
        int y = rect.height() < 0 ? rect.y() + rect.height() : rect.y();
        vr = QRect(x, y, qAbs(rect.width()), qAbs(rect.height()));
    }
    else {
        vr = QRect();
    }
    m_selectionRect = vr;
    QItemSelection is;
    for (auto index : m_paintingIndexs) {
        if (! visualRect(index).intersected(vr).isEmpty()) {
            QItemSelection s;
            s.select(index, index);
            is.merge(s, flags);
        }
    }
    selectionModel()->select(is, flags);
    this->update();
}

int TimelineView::verticalOffset() const
{
    return verticalScrollBar()->value();
}

QRegion TimelineView::visualRegionForSelection(const QItemSelection &selection) const
{
    Q_UNUSED(selection)
    return QRegion();
}

const QModelIndexList TimelineView::paintingIndexs() const
{
    return m_paintingIndexs;
}

void TimelineView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(viewport());

    QModelIndexList sis = selectionModel()->selectedIndexes();
    QStyleOptionViewItem option = viewOptions();
    painter.fillRect(this->rect(), BACKGROUND_COLOR);

    for (auto index : m_paintingIndexs) {
        if (sis.contains(index)) {
            option.state |= QStyle::State_Selected;
        }
        else {
            option.state = QStyle::State_Enabled;
        }
        option.rect = visualRect(index);
        painter.save();
        itemDelegate()->paint(&painter, option, index);
        painter.restore();
    }

    // Draw selection box
    QPainterPath bp;
    bp.addRect(m_selectionRect);
    QPen sp(BORDER_COLOR_SELECTED, 1);
    painter.setPen(sp);
    painter.drawPath(bp);
}

void TimelineView::resizeEvent(QResizeEvent *event)
{
    QAbstractItemView::resizeEvent(event);

    updateVerticalScrollbar();

    // NOTE: paintEvent may read the IndexRectList during updating
    TIMER_SINGLESHOT(100, {updateVisualRects();}, this)
}

void TimelineView::wheelEvent(QWheelEvent *e)
{
    QScrollBar *sb = verticalScrollBar();
    if (e->orientation() == Qt::Vertical &&
            sb->value() <= sb->maximum() &&
            sb->value() >= sb->minimum()) {
        if (e->angleDelta().x() == 0){
            if (e->modifiers() == Qt::ControlModifier) {
                emit changeItemSize(e->delta() > 0);
            }
            else {
                QWheelEvent ve(e->pos(), e->globalPos(), e->pixelDelta()
                               , e->angleDelta(), e->delta() * 8/*speed up*/
                               , Qt::Vertical, e->buttons(), e->modifiers());
                QApplication::sendEvent(verticalScrollBar(), &ve);
            }
        }
    }
    else {
        QApplication::sendEvent(horizontalScrollBar(), e);
    }
}

void TimelineView::mousePressEvent(QMouseEvent *e)
{
    QAbstractItemView::mousePressEvent(e);
    QModelIndex index = indexAt(e->pos());
    if (! index.isValid()) {
        return;
    }

    setSelectionMode(QAbstractItemView::ExtendedSelection);

    if (e->button() == Qt::RightButton) {
        if (selectedIndexes().length() <= 1) {
            selectionModel()->clear();
            selectionModel()->select(index, QItemSelectionModel::Select);
        }
        emit showMenu();
        return;
    }  else if (e->modifiers() == Qt::ControlModifier) {
        selectionModel()->select(index, QItemSelectionModel::ToggleCurrent);
    } else {
        selectionModel()->clear();
        selectionModel()->select(index, QItemSelectionModel::Select);
    }
}


void TimelineView::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == m_thumbTimerID) {
        killTimer(m_thumbTimerID);
        m_thumbTimerID = 0;

        QVariantList pis;

        for (QModelIndex index : m_paintingIndexs) {
            QVariantList datas = model()->data(index, Qt::DisplayRole).toList();
            if (datas.count() == 4) { // There is 4 field data inside TimelineData
                if (datas[3].value<QPixmap>().isNull()
                        && ! datas[1].toString().isEmpty()) {
                    QVariantList vs;
                    vs.append(QVariant(datas[1].toString()));
                    vs.append(QVariant(index));
                    pis.append(QVariant(vs));
                }
            }
        }

        if (! pis.isEmpty()) {
            m_watcher.setPaused(false);
            QFuture<QVariant> future = QtConcurrent::mapped(pis, genThumbnail);
            m_watcher.setFuture(future);
        }
    }

    // NOTE: QAbstractItemView handling several BasicTimer inside timerEvent
    // Please make sure it won't be stop
    QAbstractItemView::timerEvent(e);
}

/*!
 * \brief TimelineView::intersectedIndexs
 * \param rect
 * \return
 * 返回与Rect相交的所有index
 */
QModelIndexList TimelineView::visualIndexs() const
{
    QList<QModelIndex> indexs;
    if (m_irList.length() == 2) {
        return QModelIndexList()
                << m_irList.first().index
                << m_irList.last().index;
    }

    // 二分法
    int startIndex = m_irList.length() - 1;
    int endIndex = 0;
    int middleIndex = 1;
    while (middleIndex > 0 && middleIndex < m_irList.length() - 2) {
        middleIndex = (startIndex + endIndex) / 2;
        QModelIndex mi = m_irList[middleIndex].index;
        QRect mr = visualRect(mi);
        if (this->rect().intersected(mr).isEmpty()) {
            // 中点在可视区域上方
            if (mr.y() < y()) {
                endIndex = middleIndex;
            }
            // 中点在可视区域下方
            else {
                startIndex = middleIndex;
            }
        }
        // 如果某个Index对应的Rect跟可视区域有交集，则从该Index开始向两端扩展绘制
        else {
            // 向前
            int forwardIndex = middleIndex;
            while (forwardIndex >= 0 &&
                   ! this->rect().intersected(visualRect(m_irList[forwardIndex].index)).isEmpty()) {
                QModelIndex fi = m_irList[forwardIndex].index;
                indexs.insert(0, fi);
                forwardIndex --;
            }

            // 向后
            int backwardIndex = middleIndex;
            while (backwardIndex < m_irList.length() &&
                   ! this->rect().intersected(visualRect(m_irList[backwardIndex].index)).isEmpty()) {
                QModelIndex bi = m_irList[backwardIndex].index;
                indexs.append(bi);
                backwardIndex ++;
            }

            break;
        }
    }

    return indexs;
}

int TimelineView::maxColumnCount() const
{
    return this->width() / (m_itemSize + m_hItemSpacing);
}

void TimelineView::updateVerticalScrollbar()
{
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setRange(0, viewportSizeHint().height() - viewport()->height());
}

/*!
 * \brief TimelineView::updateVisualRects
 * The VisualRects should update after resized, item-changed or iconsize-changed
 * Note: it sholud consider about verticalOffset()
 */
void TimelineView::updateVisualRects()
{
    m_irList.clear();
    m_irMap.clear();

    int titleRowCount = model()->rowCount();

    int viewportHeight = /*-verticalOffset() + */m_topMargin;
    for (int trc = 0; trc < titleRowCount; trc ++) {
        // Title row
        QModelIndex titleIndex = model()->index(trc, 0);
        QRect tR(horizontalOffset(), viewportHeight,
                 this->width() - horizontalOffset() * 2, m_titleHeight);
        IndexRect ir;
        ir.index = titleIndex;
        ir.rect = tR;
        m_irList.append(ir);
        m_irMap.insert(titleIndex, tR);
        viewportHeight += m_titleHeight + m_vItemSpacing;

        // Data row
        const int columnCount = model()->columnCount(titleIndex);
        for (int c = 0; c < columnCount; c++) {
            QModelIndex dataIndex = titleIndex.child(0, c);
            int subRow = c / maxColumnCount();
            int subColumn = c % maxColumnCount();
            int y = viewportHeight
                    + subRow * (m_itemSize + m_vItemSpacing);
            int x = horizontalOffset()
                    + subColumn * (m_itemSize + m_hItemSpacing);
            QRect dR(x, y, m_itemSize, m_itemSize);
            IndexRect ir;
            ir.index = dataIndex;
            ir.rect = dR;
            m_irList.append(ir);
            m_irMap.insert(dataIndex, dR);
        }
        // Append the height of data-row to viewportHeight
        int subRowCount = qCeil(1.0 * columnCount / maxColumnCount());
        viewportHeight += subRowCount * (m_itemSize + m_vItemSpacing);
    }

    m_paintingIndexs = visualIndexs();
    emit paintingIndexsChanged();

    // Note: updateThumbnails() must call after updateVisualRects()
    updateThumbnails();
}

void TimelineView::onScrolled()
{
    updateVisualRects();
}

void TimelineView::mouseReleaseEvent(QMouseEvent *e)
{
    m_selectionRect = QRect();
    this->update();
    QAbstractItemView::mouseReleaseEvent(e);
}

void TimelineView::updateThumbnails()
{
    m_watcher.setPaused(true);
    m_watcher.cancel();

    killTimer(m_thumbTimerID);
    m_thumbTimerID = startTimer(1000);
}

void TimelineView::onThumbnailGenerated(int index)
{
    QVariantList v = m_watcher.resultAt(index).toList();
    if (v.length() != 2) return;

    QModelIndex i = v[0].value<QModelIndex>();

    TimelineModel *m = dynamic_cast<TimelineModel *>(model());
    QVariantList datas = m->data(i, Qt::DisplayRole).toList();
    if (datas.count() == 4) { // There is 4 field data inside TimelineData
        TimelineItem::ItemData data;
        data.isTitle = datas[0].toBool();
        data.path = datas[1].toString();
        data.timeline = datas[2].toString();
        data.thumbArray = v[1].toByteArray();

        m->updateData(data);
        viewport()->update();

        return;
    }
}

int TimelineView::titleHeight() const
{
    return m_titleHeight;
}

int TimelineView::itemSize() const
{
    return m_itemSize;
}

int TimelineView::topMargin() const
{
    return m_topMargin;
}

void TimelineView::setTopMargin(int topMargin)
{
    m_topMargin = topMargin;
    updateVisualRects();
}

int TimelineView::bottomMargin() const
{
    return m_bottomMargin;
}

void TimelineView::setBottomMargin(int bottomMargin)
{
    m_bottomMargin = bottomMargin;
    updateVisualRects();
}

int TimelineView::vItemSpacing() const
{
    return m_vItemSpacing;
}

void TimelineView::setVItemSpacing(int vItemSpacing)
{
    m_vItemSpacing = vItemSpacing;
    updateVisualRects();
}

int TimelineView::hItemSpacing() const
{
    return m_hItemSpacing;
}

void TimelineView::setHItemSpacing(int hItemSpacing)
{
    m_hItemSpacing = hItemSpacing;
    updateVisualRects();
}
