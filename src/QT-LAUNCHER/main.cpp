
/*---------- Header declarations for main ----------*/
#include <QApplication>
#include <QByteArray>
#include "mainwindow.h"

/*---------- Main function ----------*/
int main(int argc, char *argv[]) // these vars are imported from the OS, I think.
{
#if defined(__linux__) || defined(__unix__) 
    
/* ---------- OK YAP TIME ----------*/
    // Since modern Linux desktops default to Wayland, this is necessary 
    // to ensure the application uses the X11 backend, which is compatible with SFML's current Linux support. 
    // Without this, SFML's OpenGL context creation fails because Wayland doesn't support the required features and Qt defaults 
    // to the Wayland platform plugin. By forcing "xcb", we ensure that Qt creates an X11 window, allowing SFML to 
    // create its OpenGL context successfully. This should not affect users on Wayland since they will still 
    // be able to run the application using XWayland, albeit without native Wayland performance benefits.
    // unless the caller has already set QT_QPA_PLATFORM explicitly.

/* ---------- OK YAP TIME OVER ----------*/
    if (qgetenv("QT_QPA_PLATFORM").isEmpty())
        qputenv("QT_QPA_PLATFORM", "xcb");
#endif

/* ---------- QApplication parameters ----------*/
    QApplication app(argc, argv);
    
    // Set application style and palette
    app.setApplicationName("Aetherion");
    app.setApplicationVersion("0.4.2");
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
