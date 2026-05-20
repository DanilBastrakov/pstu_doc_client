#include <QApplication>
#include <QFile>
#include "authdialog.h"
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QFile styleFile("style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    AuthDialog authDialog;
    if (authDialog.exec() != QDialog::Accepted)
        return 0;

    MainWindow window(authDialog.token());
    window.show();

    return app.exec();
}
