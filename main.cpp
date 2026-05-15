#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollArea>
#include <QLabel>
#include <QWidget>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    auto *window = new QMainWindow();
    window->setWindowTitle("pstu_doc_client");
    window->resize(900, 600);

    auto *splitter = new QSplitter(Qt::Horizontal, window);

    auto *sidebar = new QWidget();
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);

    auto *searchBar = new QLineEdit();
    searchBar->setPlaceholderText("Search history...");
    sidebarLayout->addWidget(searchBar);

    auto *historyList = new QListWidget();
    sidebarLayout->addWidget(historyList);

    splitter->addWidget(sidebar);

    auto *mainArea = new QWidget();
    auto *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    auto *messagesWidget = new QWidget();
    auto *messagesLayout = new QVBoxLayout(messagesWidget);
    messagesLayout->setAlignment(Qt::AlignTop);
    auto *messagesLabel = new QLabel("Chat messages will appear here");
    messagesLabel->setAlignment(Qt::AlignCenter);
    messagesLayout->addWidget(messagesLabel);
    scrollArea->setWidget(messagesWidget);
    mainLayout->addWidget(scrollArea, 1);

    auto *inputBar = new QLineEdit();
    inputBar->setPlaceholderText("Type a message...");
    mainLayout->addWidget(inputBar);

    splitter->addWidget(mainArea);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({250, 650});

    window->setCentralWidget(splitter);
    window->show();

    return QApplication::exec();
}
