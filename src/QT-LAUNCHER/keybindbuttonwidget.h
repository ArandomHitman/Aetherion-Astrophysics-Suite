#pragma once
#include <QPushButton>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QString>

/**
 * KeyBindButton — a push-button that captures a single key press for keybind assignment.
 *
 * Usage:
 *   1. Display shows "[KEY]" (e.g. "[SPACE]", "[F]").
 *   2. Click the button → enters capture mode, shows "  ···  ".
 *   3. Press any non-modifier key → updates display, emits keyChanged(canonical).
 *   4. Press Escape or lose focus → cancels capture, restores previous key.
 *   5. Call setConflicting(true) to mark the button red  (duplicate key detected).
 *
 * Canonical key names match the .cfg file format used by both 2D and 3D keybinds:
 *   Single letters uppercase (A–Z), digits (0–9), SPACE, ESCAPE, ENTER, LEFT,
 *   RIGHT, UP, DOWN, TAB, BACKSPACE, F1–F12, NUM0–NUM9, [, ], ,  .  /  =  -  etc.
 */
class KeyBindButton : public QPushButton
{
    Q_OBJECT

public:
    explicit KeyBindButton(const QString &keyName, QWidget *parent = nullptr);

    QString keyName()      const { return m_keyName; }
    bool    isConflicting() const { return m_conflicting; }

    void setKeyName(const QString &name);
    void setConflicting(bool conflict);

    /// Convert a Qt key-press event to a canonical string name.
    /// Returns empty string for unknown / unrepresentable keys.
    static QString qtKeyToCanonical(int qtKey, Qt::KeyboardModifiers mods,
                                    const QString &nativeText);

signals:
    void keyChanged(const QString &newKey);

protected:
    bool         event(QEvent *e) override;          // intercept Tab before Qt moves focus
    void         mousePressEvent(QMouseEvent  *e) override;
    void         keyPressEvent(QKeyEvent      *e) override;
    void         focusOutEvent(QFocusEvent    *e) override;

private:
    QString m_keyName;
    bool    m_capturing   = false;
    bool    m_conflicting = false;

    void updateAppearance();
    void cancelCapture();
};
