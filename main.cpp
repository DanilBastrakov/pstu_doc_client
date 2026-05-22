#include <QApplication>
#include <QFile>
#include "api_client.h"
#include "authdialog.h"
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QFile styleFile("style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    ApiClient api;
    AuthDialog authDialog(&api);
    if (authDialog.exec() != QDialog::Accepted)
        return 0;

    MainWindow window(&api);
    window.show();

    return app.exec();
}
