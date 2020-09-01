// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "point.h"
#include "forms/gcodepropertiesform.h"
#include "gi/graphicsitem.h"
#include "mainwindow.h"
#include "project.h"
#include "settings.h"
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QSettings>
#include <clipper.hpp>

using namespace ClipperLib;

QVector<Pin*> Pin::m_pins;
Marker* Marker::m_markers[2] { nullptr, nullptr };

bool updateRect()
{
    QRectF rect(App::project()->getSelectedBoundingRect());
    if (rect.isEmpty()) {
        if (QMessageBox::question(nullptr, "",
                QObject::tr("There are no selected items to define the border.\n"
                            "The old border will be used."),
                QMessageBox::No, QMessageBox::Yes)
            == QMessageBox::No)
            return false;
    }
    App::layoutFrames()->updateRect();
    return true;
}

Marker::Marker(Type type)
    : m_type(type)
{
    m_markers[type] = this;
    setAcceptHoverEvents(true);
    if (m_type == Home) {
        m_path.arcTo(QRectF(QPointF(-3, -3), QSizeF(6, 6)), 0, 90);
        m_path.arcTo(QRectF(QPointF(-3, -3), QSizeF(6, 6)), 270, -90);
        setToolTip(QObject::tr("G-Code Home Point"));
    } else {
        m_path.arcTo(QRectF(QPointF(-3, -3), QSizeF(6, 6)), 90, 90);
        m_path.arcTo(QRectF(QPointF(-3, -3), QSizeF(6, 6)), 360, -90);
        setToolTip(QObject::tr("G-Code Zero Point"));
    }
    m_shape.addEllipse(QRectF(QPointF(-3, -3), QSizeF(6, 6)));
    m_rect = m_path.boundingRect();
    App::scene()->addItem(this);
}

Marker::~Marker()
{
    m_markers[m_type] = nullptr;
}

QRectF Marker::boundingRect() const
{
    if (App::scene()->drawPdf())
        return QRectF();
    return m_rect;
}

void Marker::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/)
{
    if (App::scene()->drawPdf())
        return;

    QColor c(m_type == Home ? GlobalSettings::guiColor(Colors::Home) : GlobalSettings::guiColor(Colors::Zero));
    if (option->state & QStyle ::State_MouseOver)
        c.setAlpha(255);
    if (!(flags() & QGraphicsItem::ItemIsMovable))
        c.setAlpha(static_cast<int>(c.alpha() * 0.5));

    painter->setPen(Qt::NoPen);
    painter->setBrush(c);
    painter->drawPath(m_path);
    painter->setPen(c);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPoint(0, 0), 2, 2);
}

QPainterPath Marker::shape() const
{
    if (App::scene()->drawPdf())
        return QPainterPath();
    return /*m_shape*/ m_path;
}

int Marker::type() const { return m_type ? GiPointHome : GiPointZero; }

void Marker::resetPos(bool fl)
{
    if (fl && !updateRect())
        return;

    const QRectF rect(App::layoutFrames()->boundingRect());

    if (m_type == Home)
        switch (GlobalSettings::mkrHomePos()) {
        case Qt::BottomLeftCorner:
            setPos(rect.topLeft() + GlobalSettings::mkrHomeOffset());
            break;
        case Qt::BottomRightCorner:
            setPos(rect.topRight() + GlobalSettings::mkrHomeOffset());
            break;
        case Qt::TopLeftCorner:
            setPos(rect.bottomLeft() + GlobalSettings::mkrHomeOffset());
            break;
        case Qt::TopRightCorner:
            setPos(rect.bottomRight() + GlobalSettings::mkrHomeOffset());
            break;
        default:
            setPos({});
            break;
        }
    else {
        switch (GlobalSettings::mkrZeroPos()) {
        case Qt::BottomLeftCorner:
            setPos(rect.topLeft() + GlobalSettings::mkrZeroOffset());
            break;
        case Qt::BottomRightCorner:
            setPos(rect.topRight() + GlobalSettings::mkrZeroOffset());
            break;
        case Qt::TopLeftCorner:
            setPos(rect.bottomLeft() + GlobalSettings::mkrZeroOffset());
            break;
        case Qt::TopRightCorner:
            setPos(rect.bottomRight() + GlobalSettings::mkrZeroOffset());
            break;
        default:
            setPos({});
            break;
        }
    }
    updateGCPForm();
}

void Marker::setPosX(double x)
{
    QPointF point(pos());
    if (qFuzzyCompare(point.x(), x))
        return;
    point.setX(x);
    setPos(point);
}

void Marker::setPosY(double y)
{
    QPointF point(pos());
    if (qFuzzyCompare(point.y(), y))
        return;
    point.setY(y);
    setPos(point);
}

void Marker::updateGCPForm()
{
    if (App::gCodePropertiesForm())
        App::gCodePropertiesForm()->updatePosDsbxs();
    QSettings settings;
    settings.beginGroup("Points");
    settings.setValue("pos" + QString::number(m_type), pos());
    App::project()->setChanged();
    if (m_type == Zero)
        for (Pin* pin : Pin::pins())
            pin->updateToolTip();
}

void Marker::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsItem::mouseMoveEvent(event);
    updateGCPForm();
}

void Marker::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (!(flags() & QGraphicsItem::ItemIsMovable))
        return;
    resetPos();
    //    QMatrix matrix(scene()->views().first()->matrix());
    //    matrix.translate(-pos().x(), pos().y());
    //    scene()->views().first()->setMatrix(matrix);
    updateGCPForm();
    QGraphicsItem::mouseDoubleClickEvent(event);
}

////////////////////////////////////////////////
/// \brief Pin::Pin
/// \param parent
///
Pin::Pin(QObject* parent)
    : QObject(parent)
    , QGraphicsItem(nullptr)
    , m_index(m_pins.size())
{
    setObjectName("Pin");
    setAcceptHoverEvents(true);

    if (m_index % 2) {
        m_path.arcTo(QRectF(QPointF(-3, -3), QSizeF(6, 6)), 0, 90);
        m_path.arcTo(QRectF(QPointF(-3, -3), QSizeF(6, 6)), 270, -90);
    } else {
        m_path.arcTo(QRectF(QPointF(-3, -3), QSizeF(6, 6)), 90, 90);
        m_path.arcTo(QRectF(QPointF(-3, -3), QSizeF(6, 6)), 360, -90);
    }
    m_shape.addEllipse(QRectF(QPointF(-3, -3), QSizeF(6, 6)));
    m_rect = m_path.boundingRect();

    setZValue(std::numeric_limits<qreal>::max() - m_index);
    App::scene()->addItem(this);
    m_pins.append(this);
}

Pin::~Pin()
{
}

QRectF Pin::boundingRect() const
{
    if (App::scene()->drawPdf())
        return QRectF();
    return m_rect;
}

void Pin::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/)
{
    if (App::scene()->drawPdf())
        return;

    QColor c(GlobalSettings::guiColor(Colors::Pin));
    if (option->state & QStyle ::State_MouseOver)
        c.setAlpha(255);
    if (!(flags() & QGraphicsItem::ItemIsMovable))
        c.setAlpha(static_cast<int>(c.alpha() * 0.5));
    //c.setAlpha(50);

    painter->setPen(Qt::NoPen);
    painter->setBrush(c);
    painter->drawPath(m_path);
    painter->setPen(c);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPoint(0, 0), 2, 2);
}

QPainterPath Pin::shape() const
{
    if (App::scene()->drawPdf())
        return QPainterPath();
    return m_shape;
}

void Pin::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsItem::mouseMoveEvent(event);

    QPointF pt[4] {
        m_pins[0]->pos(),
        m_pins[1]->pos(),
        m_pins[2]->pos(),
        m_pins[3]->pos()
    };

    const QPointF center(App::layoutFrames()->boundingRect().center());
    //const QPointF center(App::project()->worckRect().center());

    switch (m_index) {
    case 0:
        if (pt[0].x() > center.x())
            pt[0].rx() = center.x();
        if (pt[0].y() > center.y())
            pt[0].ry() = center.y();
        pt[2] = m_pins[2]->m_lastPos - (pt[0] - m_lastPos);
        pt[1].rx() = pt[2].x();
        pt[1].ry() = pt[0].y();
        pt[3].rx() = pt[0].x();
        pt[3].ry() = pt[2].y();
        break;
    case 1:
        if (pt[1].x() < center.x())
            pt[1].rx() = center.x();
        if (pt[1].y() > center.y())
            pt[1].ry() = center.y();
        pt[3] = m_pins[3]->m_lastPos - (pt[1] - m_lastPos);
        pt[0].rx() = pt[3].x();
        pt[0].ry() = pt[1].y();
        pt[2].rx() = pt[1].x();
        pt[2].ry() = pt[3].y();
        break;
    case 2:
        if (pt[2].x() < center.x())
            pt[2].rx() = center.x();
        if (pt[2].y() < center.y())
            pt[2].ry() = center.y();
        pt[0] = m_pins[0]->m_lastPos - (pt[2] - m_lastPos);
        pt[1].rx() = pt[2].x();
        pt[1].ry() = pt[0].y();
        pt[3].rx() = pt[0].x();
        pt[3].ry() = pt[2].y();
        break;
    case 3:
        if (pt[3].x() > center.x())
            pt[3].rx() = center.x();
        if (pt[3].y() < center.y())
            pt[3].ry() = center.y();
        pt[1] = m_pins[1]->m_lastPos - (pt[3] - m_lastPos);
        pt[0].rx() = pt[3].x();
        pt[0].ry() = pt[1].y();
        pt[2].rx() = pt[1].x();
        pt[2].ry() = pt[3].y();
        break;
    }

    for (int i = 0; i < 4; ++i)
        m_pins[i]->setPos(pt[i]);

    App::project()->setChanged();
}

void Pin::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    if (!(flags() & QGraphicsItem::ItemIsMovable))
        return;
    resetPos();
    QGraphicsItem::mouseDoubleClickEvent(event);
}

void Pin::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    for (int i = 0; i < 4; ++i)
        m_pins[i]->m_lastPos = m_pins[i]->pos();
    QGraphicsItem::mousePressEvent(event);
}

int Pin::type() const { return GiPin; }

QVector<Pin*> Pin::pins() { return m_pins; }

void Pin::resetPos(bool fl)
{
    if (fl)
        if (!updateRect())
            return;

    const QPointF offset(GlobalSettings::mkrPinOffset());
    const QRectF rect(App::layoutFrames()->boundingRect()); //App::project()->worckRect()

    QPointF pt[] {
        QPointF(rect.topLeft() + QPointF(-offset.x(), -offset.y())),
        QPointF(rect.topRight() + QPointF(+offset.x(), -offset.y())),
        QPointF(rect.bottomRight() + QPointF(+offset.x(), +offset.y())),
        QPointF(rect.bottomLeft() + QPointF(-offset.x(), +offset.y())),
    };

    const QPointF center(rect.center());

    if (pt[0].x() > center.x())
        pt[0].rx() = center.x();
    if (pt[0].y() > center.y())
        pt[0].ry() = center.y();
    if (pt[1].x() < center.x())
        pt[1].rx() = center.x();
    if (pt[1].y() > center.y())
        pt[1].ry() = center.y();
    if (pt[2].x() < center.x())
        pt[2].rx() = center.x();
    if (pt[2].y() < center.y())
        pt[2].ry() = center.y();
    if (pt[3].x() > center.x())
        pt[3].rx() = center.x();
    if (pt[3].y() < center.y())
        pt[3].ry() = center.y();

    for (int i = 0; i < 4; ++i)
        m_pins[i]->setPos(pt[i]);

    App::project()->setChanged();
}

void Pin::updateToolTip()
{
    const QPointF p(pos() - Marker::get(Marker::Zero)->pos());
    setToolTip(QObject::tr("Pin %1\nX %2:Y %3").arg(m_pins.indexOf(this) + 1).arg(p.x()).arg(p.y()));
}

void Pin::setPos(const QPointF& pos)
{
    QGraphicsItem::setPos(pos);
    updateToolTip();
}

////////////////////////////////////////////////
LayoutFrames::LayoutFrames()
{
    if (App::mInstance->m_layoutFrames) {
        QMessageBox::critical(nullptr, "Err", "You cannot create class LayoutFrames more than 2 times!!!");
        exit(1);
    }
    setZValue(-std::numeric_limits<double>::max());
    App::mInstance->m_layoutFrames = this;
    App::scene()->addItem(this);
}

LayoutFrames::~LayoutFrames()
{
    App::mInstance->m_layoutFrames = nullptr;
}

int LayoutFrames::type() const
{
    return GiLayoutFrames;
}

QRectF LayoutFrames::boundingRect() const
{
    if (App::scene()->drawPdf())
        return QRectF();
    return m_path.boundingRect();
}

void LayoutFrames::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setBrush(Qt::NoBrush);
    QPen pen(QColor(100, 0, 100), 0.0);
    pen.setWidthF(2.0 * App::graphicsView()->scaleFactor());
    pen.setJoinStyle(Qt::MiterJoin);
    //    pen.setStyle(Qt::CustomDashLine);
    //    pen.setCapStyle(Qt::FlatCap);
    //    pen.setDashPattern({ 2.0, 5.0 });
    painter->setPen(pen);
    painter->drawPath(m_path);
    painter->restore();
}

void LayoutFrames::updateRect(bool fl)
{
    QPainterPath path;
    QRectF rect(App::project()->worckRect());
    for (int x = 0; x < App::project()->stepsX(); ++x) {
        for (int y = 0; y < App::project()->stepsY(); ++y) {
            path.addRect(rect.translated(
                (rect.width() + App::project()->spaceX()) * x,
                (rect.height() + App::project()->spaceY()) * y));
        }
    }
    m_path = path;
    QGraphicsItem::update();
    scene()->setSceneRect(scene()->itemsBoundingRect());
    if (fl) {
        Marker::get(Marker::Home)->resetPos(false);
        Marker::get(Marker::Zero)->resetPos(false);
        Pin::resetPos(false);
    }
}
