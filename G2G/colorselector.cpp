// This is an open source non-commercial project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "colorselector.h"
#include "ui_colorselector.h"

#include <QColorDialog>
#include <QDebug>
#include <qevent.h>

ColorSelector::ColorSelector(QColor& color, const QColor& defaultColor, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ColorSelector)
    , m_color(color)
    , m_defaultColor(std::move(defaultColor))
{
    ui->setupUi(this);
    ui->frSelectColor->installEventFilter(this);
    ui->frSelectColor->setStyleSheet("QFrame { background: " + m_color.name(QColor::HexArgb) + " }");
}

ColorSelector::~ColorSelector()
{
    delete ui;
}

void ColorSelector::on_pbResetColor_clicked()
{
    m_color = m_defaultColor;
    ui->frSelectColor->setStyleSheet("QFrame { background: " + m_color.name(QColor::HexArgb) + " }");
}

bool ColorSelector::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->frSelectColor) {
        if (event->type() == QEvent::MouseButtonPress) {
            QColorDialog dialog(m_color);
            dialog.setOption(QColorDialog::ShowAlphaChannel, true);
            QColor color(m_color);
            connect(&dialog, &QColorDialog::currentColorChanged, [&color](const QColor& c) { color = c; });
            if (dialog.exec() && m_color != color) {
                //qDebug() << m_color << dialog.currentColor();
                m_color = color; //dialog.currentColor();
                ui->frSelectColor->setStyleSheet("QFrame { background: " + m_color.name(QColor::HexArgb) + " }");
            }
            return true;
        } else {
            return false;
        }
    } else {
        return QWidget::eventFilter(watched, event);
    }
}
