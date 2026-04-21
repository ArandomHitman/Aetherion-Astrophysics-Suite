#ifndef SFML_CANVAS_H
#define SFML_CANVAS_H

// platform.hpp MUST come before <SFML/Graphics.hpp>:
// On Linux, SFML/Graphics.hpp transitively includes SFML/OpenGL.hpp -> GL/gl.h.
// GLEW (used on Linux/Windows) requires being included before any other GL header.
// Putting platform.hpp here means every TU that includes sfml_canvas.h is covered.
#include "platform.hpp"

#include <QWidget>
#include <QTimer>
#include <SFML/Graphics.hpp>
#include <memory>

/**
 * QSFMLCanvas - Embedded SFML rendering surface in Qt
 * Allows SFML windows to be rendered within Qt widgets,
 * enabling multiple simulation windows in a single Qt application.
 */
class QSFMLCanvas : public QWidget, public sf::RenderWindow
{
    Q_OBJECT

public:
    explicit QSFMLCanvas(QWidget *parent = nullptr, const QSize &size = QSize(400, 400),
                        sf::ContextSettings ctxSettings = sf::ContextSettings{});
    virtual ~QSFMLCanvas();

    // Called when the widget is shown - should initialize SFML resources
    virtual void onInit() {}
    
    // Called every frame - override to draw content
    virtual void onUpdate() {}
    
    // Input event handlers - override in subclasses
    virtual void onMouseMoved(float x, float y) {}
    virtual void onMousePressed(sf::Mouse::Button button, float x, float y) {}
    virtual void onMouseReleased(sf::Mouse::Button button, float x, float y) {}
    virtual void onKeyPressed(sf::Keyboard::Key code) {}
    virtual void onKeyReleased(sf::Keyboard::Key code) {}
    virtual void onMouseWheelScrolled(float delta, float x, float y) {}

protected:
    QPaintEngine *paintEngine() const override { return nullptr; }
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void initializeSFML();

    sf::ContextSettings contextSettings_;
    bool    isInitialized;
    QTimer *renderTimer_;
};

#endif // SFML_CANVAS_H
