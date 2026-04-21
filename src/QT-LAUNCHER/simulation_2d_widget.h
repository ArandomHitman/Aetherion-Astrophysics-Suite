#ifndef SIMULATION_2D_WIDGET_H
#define SIMULATION_2D_WIDGET_H

#include "sfml_canvas.h"
#include "2D-utils/key_config_2d.hpp"
#include <QJsonObject>
#include <memory>
#include <vector>
#include <string>

// Forward declarations — full types included only in the .cpp
class  Simulation;
struct Camera;
class  Renderer;
struct UIState;

/**
 * Simulation2DWidget — embeds the full 2D black hole simulation inside a Qt tab.
 * Mirrors the BlackHole2D main() render loop inside onUpdate(), driven by
 * QSFMLCanvas::paintEvent (which calls onUpdate() then display()).
 */
class Simulation2DWidget : public QSFMLCanvas
{
    Q_OBJECT

public:
    explicit Simulation2DWidget(const QString &workspaceName, QWidget *parent = nullptr);
    ~Simulation2DWidget();   // Non-inline: defined in .cpp where all types are complete

    void setWorkspaceName(const QString &name);

    /// Serialize current simulation + UI state for persistence.
    QJsonObject getState() const;

    /// Store state to be applied once onInit() runs (call before the widget is shown).
    void setPendingState(const QJsonObject &obj);

protected:
    void onInit() override;
    void onUpdate() override;
    void onKeyPressed(sf::Keyboard::Key code) override;
    void onKeyReleased(sf::Keyboard::Key code) override;
    void onMouseMoved(float x, float y) override;
    void onMousePressed(sf::Mouse::Button button, float x, float y) override;
    void onMouseWheelScrolled(float delta, float x, float y) override;

private:
    std::unique_ptr<Simulation> sim_;
    std::unique_ptr<Camera>     camera_;
    std::unique_ptr<Renderer>   renderer_;
    std::unique_ptr<UIState>    ui_;
    sf::Clock                   clock_;
    std::vector<sf::Vertex>     rayVertScratch_;
    KeyConfig2D                 keyConfig_;   ///< loaded from blackhole2d_keybinds.cfg
    QString                     m_workspaceName = "untitled_data";
    bool                        hasPendingState_ = false;
    QJsonObject                 pendingState_;
};

#endif // SIMULATION_2D_WIDGET_H
