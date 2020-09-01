#pragma once
//#ifndef MAINWINDOW_H
//#define MAINWINDOW_H

#include "ui_mainwindow.h"
#include <QSettings>
#include <QThread>
#include <qevent.h>

namespace Gerber {
class Parser;
}
namespace Excellon {
class Parser;
}
namespace GCode {
class File;
}

class DockWidget;
class Project;
class QProgressDialog;
class Scene;

class MainWindow : public QMainWindow, private Ui::MainWindow {
    Q_OBJECT
    friend void TreeView::on_doubleClicked(const QModelIndex&);

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // QMainWindow interface
    QMenu* createPopupMenu() override;

signals:
    void parseGerberFile(const QString& filename);
    void parseExcellonFile(const QString& filename);

private:
    enum { MaxRecentFiles = 20 };
    DockWidget* dockWidget = nullptr;

    Gerber::Parser* gerberParser;
    Excellon::Parser* excellonParser;

    QAction* m_closeAllAct = nullptr;
    QAction* recentFileActs[MaxRecentFiles + 1];
    QAction* recentFileSeparator = nullptr;
    QAction* recentFileSubMenuAct = nullptr;

    QMenu* fileMenu = nullptr;
    QMenu* helpMenu = nullptr;
    QMenu* recentMenu = nullptr;
    QMenu* serviceMenu = nullptr;

    QString lastPath;
    QThread parserThread;

    QToolBar* fileToolBar = nullptr;
    QToolBar* toolpathToolBar = nullptr;
    QToolBar* zoomToolBar = nullptr;

    Scene* scene;

    Project* m_project;
    bool openFlag;

    QMap<int, QAction*> toolpathActionList;

    QMap<QString, QProgressDialog*> m_progressDialogs;

    inline QString fileKey();
    inline QString recentFilesKey();
    void about();
    bool closeProject();
    template <class T>
    void createDockWidget(/*QWidget* dwContent,*/ int type);
    void createPinsPath();
    void fileError(const QString& fileName, const QString& error);
    void fileProgress(const QString& fileName, int max, int value);
    void initWidgets();
    void onCustomContextMenuRequested(const QPoint& pos);
    void openRecentFile();
    void prependToRecentFiles(const QString& fileName);
    void printDialog();
    void readSettings();
    void redo();
    void resetToolPathsActions();
    void selectAll();
    void setRecentFilesVisible(bool visible);
    void updateRecentFileActions();
    void writeRecentFiles(const QStringList& files, QSettings& settings);
    void writeSettings();

    // create actions
    void createActions();
    void createActionsFile();
    void createActionsEdit();
    void createActionsService();
    void createActionsHelp();
    void createActionsZoom();
    void createActionsToolPath();
    void createActionsGraphics();

    QString strippedName(const QString& fullFileName);
    QStringList readRecentFiles(QSettings& settings);
    bool hasRecentFiles();
    void newFile();
    void open();
    bool save();
    bool saveAs();
    void documentWasModified();
    bool maybeSave();

    void editGcFile(GCode::File* file);

public:
    void loadFile(const QString& fileName);

private:
    bool saveFile(const QString& fileName);
    void setCurrentFile(const QString& fileName);
    void addFileToPro(AbstractFile* file);

    // QWidget interface
protected:
    void closeEvent(QCloseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void showEvent(QShowEvent* event) override;
};

class DockWidget : public QDockWidget {
    Q_OBJECT
public:
    explicit DockWidget(QWidget* parent = nullptr)
        : QDockWidget(parent)
    {
        hide();
        setVisible(false);
    }
    ~DockWidget() override = default;

    // QWidget interface
protected:
    void closeEvent(QCloseEvent* event) override
    {
        delete widget();
        event->accept();
    }

    void showEvent(QShowEvent* event) override
    {
        event->ignore();
        //        close();
        //        QDockWidget::showEvent(event);
        if (widget() == nullptr)
            QTimer::singleShot(1, this, &QDockWidget::close);
    }
};

//#endif // MAINWINDOW_H
