#include <QApplication>
#include "principal.h"

int main( int argc, char **argv )  {
    QApplication app( argc, argv );

    Principal principal;
    principal.showMaximized();

    return app.exec();
}
