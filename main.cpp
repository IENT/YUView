#include "yuviewapp.h"

int main(int argc, char *argv[])
{
    // set correct OpenGL buffering mode
    QGLFormat fmt;
    fmt.setStereo( true );
    QGLFormat::setDefaultFormat(fmt);

    YUViewApp a(argc, argv);

    return a.exec();
}
