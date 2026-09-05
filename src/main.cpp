#include "MainWindow.h"
#include "Theme.h"
#include "Icons.h"

#include <QApplication>
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

    window.show();

    if (parser.isSet(screenshotOption)) {
        app.processEvents();
        const QString screenshotPath = parser.value(screenshotOption);
        QPixmap pixmap = window.grab();
        pixmap.save(screenshotPath);
        return 0;
    }

    return app.exec();
}
