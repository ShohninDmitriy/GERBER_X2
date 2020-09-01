#pragma once
//#ifndef POCKETRASTERFORM_H
//#define POCKETRASTERFORM_H

#include "formsutil/formsutil.h"

namespace Ui {
class PocketRasterForm;
}

class PocketRasterForm : public FormsUtil {
    Q_OBJECT

public:
    explicit PocketRasterForm(QWidget* parent = nullptr);
    ~PocketRasterForm();

private slots:
    void on_leName_textChanged(const QString& arg1);

private:
    Ui::PocketRasterForm* ui;

    int direction = 0;
    void updatePixmap();
    void rb_clicked();
    const QStringList names;
    const QStringList pixmaps;
    // QWidget interface
protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

    // FormsUtil interface
protected:
    void createFile() override;
    void updateName() override;

public:
    void editFile(GCode::File* file) override;
};

//#endif // POCKETRASTERFORM_H
