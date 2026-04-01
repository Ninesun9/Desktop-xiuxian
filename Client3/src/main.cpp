#include <QApplication>
#include "petwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("XiuxianPet");
    app.setApplicationVersion("1.0.0");
    app.setQuitOnLastWindowClosed(false);

    PetWindow window;
    window.show();

    return app.exec();
}
