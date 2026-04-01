#include <QApplication>
#include "PetWidget.h"

int main(int argc, char *argv[]) {

    QApplication app(argc, argv);
    
    PetWidget w;
    w.show();
    
    return app.exec();
}
