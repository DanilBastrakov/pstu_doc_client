#include <QApplication>
#include "authdialog.h"
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    AuthDialog authDialog;
    if (authDialog.exec() != QDialog::Accepted)
        return 0;

    MainWindow window(authDialog.token());
    window.show();

    return app.exec();
}
