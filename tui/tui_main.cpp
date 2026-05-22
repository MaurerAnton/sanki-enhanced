#include "tui_app.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLoggingCategory>

int main(int argc, char* argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("Sanki");

    QLoggingCategory::setFilterRules("qt.*=false");

    qRegisterMetaTypeStreamOperators<QList<QString>>("QList<QString>");
    qRegisterMetaTypeStreamOperators<DeckModes>("DeckModes");
    qRegisterMetaTypeStreamOperators<core>("core");
    qRegisterMetaTypeStreamOperators<times>("times");
    qRegisterMetaTypeStreamOperators<deckOptions>("deckOptions");
    qRegisterMetaTypeStreamOperators<card>("card");
    qRegisterMetaTypeStreamOperators<sessionStr>("sessionStr");
    qRegisterMetaTypeStreamOperators<box>("box");
    qRegisterMetaTypeStreamOperators<sm2CardData>("sm2CardData");
    qRegisterMetaTypeStreamOperators<sm2Config>("sm2Config");

    qDebug() << "Sanki TUI starting...";

    TuiApp app;
    app.run();

    qDebug() << "Sanki TUI exited. Goodbye.";
    return 0;
}
