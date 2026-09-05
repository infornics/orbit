#include "app/MainWindow.h"
#include "ui/Theme.h"
#include "ui/Icons.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QFileInfo>
#include <QDir>
#include <QIcon>
#include <QCommandLineParser>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("Orbit");
    app.setApplicationDisplayName("Orbit");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("OrbitEditor");
    app.setWindowIcon(Orbit::Icons::orbit(32));

    // Apply global UI font and stylesheet
    app.setFont(Orbit::Theme::uiFont(10));
    app.setStyleSheet(Orbit::Theme::applicationStyleSheet());

    Orbit::MainWindow window;

    // Command line argument handling
    QCommandLineParser parser;
    parser.setApplicationDescription("Orbit — A fast, minimalist code and text editor");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption screenshotOption("screenshot", "Save window screenshot to file and exit", "file");
    parser.addOption(screenshotOption);
    QCommandLineOption openMenuOption("open-menu", "Open specified menu before screenshot", "name");
    parser.addOption(openMenuOption);
    QCommandLineOption autoSaveOption("enable-autosave", "Enable autosave");
    parser.addOption(autoSaveOption);
    QCommandLineOption showDialogOption("show-unsaved-dialog", "Show unsaved dialog for screenshot");
    parser.addOption(showDialogOption);
    QCommandLineOption showAntigravityOption("show-antigravity", "Show the Antigravity panel");
    parser.addOption(showAntigravityOption);
    parser.addPositionalArgument("path", "Initial file or directory to open", "[path]");
    parser.process(app);

    const QStringList positionalArgs = parser.positionalArguments();
    if (!positionalArgs.isEmpty()) {
        const QString path = QFileInfo(positionalArgs.first()).absoluteFilePath();
        QFileInfo info(path);
        if (info.isDir()) {
            window.openFolder(path);
        } else {
            window.openFolder(info.absolutePath());
            window.openFile(path);
        }
    }

    if (parser.isSet(autoSaveOption)) {
        for (auto *action : window.menuBar()->actions()) {
            if (auto *menu = action->menu()) {
                for (auto *item : menu->actions()) {
                    if (item->text().contains("Auto Save")) {
                        item->setChecked(true);
                    }
                }
            }
        }
    }

    window.show();

    if (parser.isSet(showAntigravityOption)) {
        QMetaObject::invokeMethod(&window, "onToggleAntigravity");
    }

    if (parser.isSet(showDialogOption)) {
        QMessageBox msgBox(&window);
        msgBox.setWindowTitle("Unsaved Changes — Orbit");
        msgBox.setText(QObject::tr("Do you want to save changes to 'package.json'?"));
        msgBox.setInformativeText(QObject::tr("Your changes will be lost if you don't save them."));
        msgBox.setIcon(QMessageBox::NoIcon);

        QPushButton *saveBtn = msgBox.addButton(QObject::tr("Save"), QMessageBox::AcceptRole);
        QPushButton *discardBtn = msgBox.addButton(QObject::tr("Don't Save"), QMessageBox::DestructiveRole);
        QPushButton *cancelBtn = msgBox.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);

        msgBox.setDefaultButton(saveBtn);
        saveBtn->setIcon(QIcon());
        discardBtn->setIcon(QIcon());
        cancelBtn->setIcon(QIcon());

        msgBox.show();
        app.processEvents();
        if (parser.isSet(screenshotOption)) {
            QPixmap pix = msgBox.grab();
            pix.save(parser.value(screenshotOption));
            return 0;
        }
    }

    if (parser.isSet(screenshotOption)) {
        app.processEvents();
        const QString screenshotPath = parser.value(screenshotOption);
        if (parser.isSet(openMenuOption)) {
            const QString menuName = parser.value(openMenuOption);
            for (auto *action : window.menuBar()->actions()) {
                if (auto *menu = action->menu()) {
                    if (menu->title().contains(menuName, Qt::CaseInsensitive)) {
                        menu->show();
                        app.processEvents();
                        QPixmap menuPix = menu->grab();
                        menuPix.save(screenshotPath);
                        return 0;
                    }
                }
            }
        }
        QPixmap pixmap = window.grab();
        pixmap.save(screenshotPath);
        return 0;
    }

    return app.exec();
}
