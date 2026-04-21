#include "sfml_canvas.h"  // brings in platform.hpp first, then SFML
#include <cstdint>
#include <QWidget>
#include <QShowEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QDebug>

QSFMLCanvas::QSFMLCanvas(QWidget *parent, const QSize &size, sf::ContextSettings ctxSettings)
    : QWidget(parent), isInitialized(false), contextSettings_(ctxSettings)
    , renderTimer_(new QTimer(this))
{
    // Set minimum and initial size
    QWidget::setMinimumSize(100, 100);
    QWidget::resize(size);

    // Enable mouse tracking for mouse move events
    setMouseTracking(true);

    // Set focus policy to receive keyboard events
    setFocusPolicy(Qt::StrongFocus);

    // Don't erase background - SFML will handle rendering
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    // Force Qt to allocate a real native window handle (NSView / HWND / X11 window)
    // for this widget rather than sharing the top-level window's handle.
    // Without this, winId() returns the main window's handle and SFML creates its
    // rendering surface over the entire application instead of just this widget.
    setAttribute(Qt::WA_NativeWindow);

    // On X11, tell Qt not to use a backing store for this widget and to skip
    // compositing it: Qt draws its own UI (tab bar, menus) and the native GL
    // surface via separate paths.  Without this, Qt's backing-store compositor
    // can paint over (or behind) the SFML surface, causing visible overlap.
    // This attribute is a no-op on Wayland/macOS/Windows, so it's safe everywhere.
    setAttribute(Qt::WA_PaintOnScreen);

    // Prevent Qt from creating native (X11) windows for intermediate parent
    // widgets (QTabWidget pane, QStackedWidget, etc.).  If those parents also
    // become native they get their own backing stores which can occlude the
    // child SFML window during repaints.
    setAttribute(Qt::WA_DontCreateNativeAncestors);

    // Drive the render loop at ~60 fps via a timer rather than re-queuing
    // update() inside paintEvent.  The unconstrained spin floods the X11
    // compositor and is the primary cause of the stutter on Linux.  The timer
    // is started/stopped with the widget visibility so we don't burn CPU
    // rendering hidden tabs.
    renderTimer_->setInterval(16); // ~60 fps
    connect(renderTimer_, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
}

QSFMLCanvas::~QSFMLCanvas()
{
    sf::RenderWindow::close();
}

void QSFMLCanvas::showEvent(QShowEvent *event)
{
    if (!isInitialized) {
        initializeSFML();
        onInit();
        isInitialized = true;
    }
    renderTimer_->start();
    QWidget::showEvent(event);
}

void QSFMLCanvas::initializeSFML()
{
    // Get the native window ID for this Qt widget.
    // sf::WindowHandle is void* on macOS but unsigned long on Linux, so the
    // cast strategy differs: reinterpret_cast is required for integer→pointer on
    // macOS, while static_cast suffices for integer→integer on Linux/Windows.
#if defined(__APPLE__)
    sf::RenderWindow::create(reinterpret_cast<sf::WindowHandle>(static_cast<std::uintptr_t>(winId())), contextSettings_);
#else
    sf::RenderWindow::create(static_cast<sf::WindowHandle>(static_cast<unsigned long>(winId())), contextSettings_);
#endif
}

void QSFMLCanvas::hideEvent(QHideEvent *event)
{
    // Stop rendering while the widget is hidden (e.g. tab switch) so we
    // don't burn CPU/GPU on a surface the user can't see.
    renderTimer_->stop();
    // Release the OpenGL context so another context (e.g. the other sim tab)
    // can become active without GL state corruption.
    if (isInitialized)
        (void)sf::RenderWindow::setActive(false);
    QWidget::hideEvent(event);
}

void QSFMLCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (isInitialized) {
        // Reclaim the OpenGL context — Qt may have activated another context
        // (e.g. for QChartView) since the last frame, which corrupts the SFML
        // vertex state and causes paths to render as straight lines.
        (void)sf::RenderWindow::setActive(true);
        onUpdate();
        display();
        // Release the context so Qt widgets (QChartView, styled widgets)
        // that use their own GL contexts don't inherit our state.
        (void)sf::RenderWindow::setActive(false);
    }
    // Rendering is driven by renderTimer_ — no update() call here.
}

void QSFMLCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    if (isInitialized) {
        // Update SFML view to match new size
        QSize size = event->size();
        sf::RenderWindow::setSize({static_cast<unsigned int>(size.width()),
                                   static_cast<unsigned int>(size.height())});
    }
}

void QSFMLCanvas::mouseMoveEvent(QMouseEvent *event)
{
    onMouseMoved(event->pos().x(), event->pos().y());
    QWidget::mouseMoveEvent(event);
}

void QSFMLCanvas::mousePressEvent(QMouseEvent *event)
{
    sf::Mouse::Button button = sf::Mouse::Button::Left;
    if (event->button() == Qt::RightButton)
        button = sf::Mouse::Button::Right;
    else if (event->button() == Qt::MiddleButton)
        button = sf::Mouse::Button::Middle;
    
    onMousePressed(button, event->pos().x(), event->pos().y());
    QWidget::mousePressEvent(event);
}

void QSFMLCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    sf::Mouse::Button button = sf::Mouse::Button::Left;
    if (event->button() == Qt::RightButton)
        button = sf::Mouse::Button::Right;
    else if (event->button() == Qt::MiddleButton)
        button = sf::Mouse::Button::Middle;
    
    onMouseReleased(button, event->pos().x(), event->pos().y());
    QWidget::mouseReleaseEvent(event);
}

// Helper: translate a Qt key code to an SFML key code.
static sf::Keyboard::Key qtKeyToSFML(int qtKey)
{
    // Letter keys A–Z (Qt::Key_A == 65, sf::Keyboard::Key::A == 0)
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return static_cast<sf::Keyboard::Key>(
            static_cast<int>(sf::Keyboard::Key::A) + (qtKey - Qt::Key_A));

    // Digit keys 0–9 (Qt::Key_0 == 48, sf::Keyboard::Key::Num0 == 26)
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return static_cast<sf::Keyboard::Key>(
            static_cast<int>(sf::Keyboard::Key::Num0) + (qtKey - Qt::Key_0));

    switch (qtKey) {
        case Qt::Key_Left:         return sf::Keyboard::Key::Left;
        case Qt::Key_Right:        return sf::Keyboard::Key::Right;
        case Qt::Key_Up:           return sf::Keyboard::Key::Up;
        case Qt::Key_Down:         return sf::Keyboard::Key::Down;
        case Qt::Key_Escape:       return sf::Keyboard::Key::Escape;
        case Qt::Key_Space:        return sf::Keyboard::Key::Space;
        case Qt::Key_Return:       return sf::Keyboard::Key::Enter;
        case Qt::Key_Home:         return sf::Keyboard::Key::Home;
        case Qt::Key_End:          return sf::Keyboard::Key::End;
        case Qt::Key_PageUp:       return sf::Keyboard::Key::PageUp;
        case Qt::Key_PageDown:     return sf::Keyboard::Key::PageDown;
        case Qt::Key_Control:      return sf::Keyboard::Key::LControl;
        case Qt::Key_Shift:        return sf::Keyboard::Key::LShift;
        case Qt::Key_Slash:        return sf::Keyboard::Key::Slash;
        case Qt::Key_Question:     return sf::Keyboard::Key::Slash;   // ? = Shift+/
        case Qt::Key_BracketLeft:  return sf::Keyboard::Key::LBracket;
        case Qt::Key_BracketRight: return sf::Keyboard::Key::RBracket;
        case Qt::Key_Period:       return sf::Keyboard::Key::Period;
        case Qt::Key_Comma:        return sf::Keyboard::Key::Comma;
        case Qt::Key_Minus:        return sf::Keyboard::Key::Hyphen;
        case Qt::Key_Equal:        return sf::Keyboard::Key::Equal;
        default:                   return sf::Keyboard::Key::Unknown;
    }
}

void QSFMLCanvas::keyPressEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat()) {
        sf::Keyboard::Key key = qtKeyToSFML(event->key());
        onKeyPressed(key);
    }
    QWidget::keyPressEvent(event);
}

void QSFMLCanvas::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat()) {
        sf::Keyboard::Key key = qtKeyToSFML(event->key());
        onKeyReleased(key);
    }
    QWidget::keyReleaseEvent(event);
}

void QSFMLCanvas::wheelEvent(QWheelEvent *event)
{
    // Scroll wheel delta in degrees / 120 = number of notches
    float delta = event->angleDelta().y() / 120.0f;
    const QPointF p = event->position();
    onMouseWheelScrolled(delta, static_cast<float>(p.x()), static_cast<float>(p.y()));
    QWidget::wheelEvent(event);
}
