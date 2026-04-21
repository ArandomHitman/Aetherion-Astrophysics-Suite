#include "keybindbuttonwidget.h"
#include <QEvent>

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────

KeyBindButton::KeyBindButton(const QString &keyName, QWidget *parent)
    : QPushButton(parent), m_keyName(keyName)
{
    setFocusPolicy(Qt::StrongFocus);
    setFixedHeight(28);
    setMinimumWidth(90);
    setMaximumWidth(150);
    setCursor(Qt::PointingHandCursor);
    updateAppearance();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public setters
// ─────────────────────────────────────────────────────────────────────────────

void KeyBindButton::setKeyName(const QString &name)
{
    m_keyName   = name;
    m_capturing = false;
    updateAppearance();
}

void KeyBindButton::setConflicting(bool conflict)
{
    if (m_conflicting == conflict) return;
    m_conflicting = conflict;
    updateAppearance();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Event overrides
// ─────────────────────────────────────────────────────────────────────────────

bool KeyBindButton::event(QEvent *e)
{
    // Intercept Tab / BackTab while capturing so Qt doesn't move focus away.
    if (m_capturing && e->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(e);
        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) {
            keyPressEvent(ke);
            return true;
        }
    }
    return QPushButton::event(e);
}

void KeyBindButton::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && !m_capturing) {
        m_capturing = true;
        setFocus(Qt::MouseFocusReason);
        updateAppearance();
        // Don't call super — prevents the QPushButton click state machine from
        // firing clicked() on the subsequent mouse-release while we're capturing.
        return;
    }
    QPushButton::mousePressEvent(e);
}

void KeyBindButton::keyPressEvent(QKeyEvent *e)
{
    if (!m_capturing) {
        QPushButton::keyPressEvent(e);
        return;
    }

    const int key = e->key();

    // Escape → cancel capture, keep existing key.
    if (key == Qt::Key_Escape) {
        cancelCapture();
        return;
    }

    // Ignore standalone modifier keys — wait for a real key.
    if (key == Qt::Key_Control || key == Qt::Key_Shift ||
        key == Qt::Key_Alt     || key == Qt::Key_Meta) {
        return;
    }

    const QString canonical = qtKeyToCanonical(key, e->modifiers(), e->text());
    if (canonical.isEmpty()) {
        cancelCapture();
        return;
    }

    m_keyName   = canonical;
    m_capturing = false;
    updateAppearance();
    emit keyChanged(canonical);
}

void KeyBindButton::focusOutEvent(QFocusEvent *e)
{
    if (m_capturing) cancelCapture();
    QPushButton::focusOutEvent(e);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Private helpers
// ─────────────────────────────────────────────────────────────────────────────

void KeyBindButton::cancelCapture()
{
    m_capturing = false;
    updateAppearance();
}

void KeyBindButton::updateAppearance()
{
    if (m_capturing) {
        setText("  \xC2\xB7\xC2\xB7\xC2\xB7  "); // UTF-8 "  ···  "
        setStyleSheet(
            "QPushButton {"
            "  background-color: #111e2e;"
            "  color: #6699cc;"
            "  border: 1px solid #4477aa;"
            "  border-radius: 4px;"
            "  font-weight: bold;"
            "  font-family: monospace;"
            "  padding: 0 10px;"
            "}");
    } else if (m_conflicting) {
        setText("[" + m_keyName + "]");
        setStyleSheet(
            "QPushButton {"
            "  background-color: #260a0a;"
            "  color: #ff5555;"
            "  border: 1px solid #aa2222;"
            "  border-radius: 4px;"
            "  font-weight: bold;"
            "  font-family: monospace;"
            "  padding: 0 10px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #330d0d;"
            "  border-color: #cc3333;"
            "}");
    } else {
        setText("[" + m_keyName + "]");
        setStyleSheet(
            "QPushButton {"
            "  background-color: #141424;"
            "  color: #b0bcd8;"
            "  border: 1px solid #333a55;"
            "  border-radius: 4px;"
            "  font-weight: bold;"
            "  font-family: monospace;"
            "  padding: 0 10px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #1c1c36;"
            "  border-color: #5577aa;"
            "  color: #d0daf8;"
            "}");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Static: Qt key → canonical string
// ─────────────────────────────────────────────────────────────────────────────

QString KeyBindButton::qtKeyToCanonical(int qtKey, Qt::KeyboardModifiers mods,
                                        const QString &nativeText)
{
    // Numpad: Qt::KeypadModifier is set for numpad digit presses.
    if (mods & Qt::KeypadModifier) {
        if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
            return QString("NUM%1").arg(qtKey - Qt::Key_0);
    }

    // Regular letter keys A–Z.
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return QString(QChar('A' + (qtKey - Qt::Key_A)));

    // Digit row 0–9.
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return QString(QChar('0' + (qtKey - Qt::Key_0)));

    // Function keys F1–F12.
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12)
        return QString("F%1").arg(qtKey - Qt::Key_F1 + 1);

    switch (qtKey) {
        case Qt::Key_Space:        return "SPACE";
        case Qt::Key_Return:       return "ENTER";
        case Qt::Key_Enter:        return "ENTER";
        case Qt::Key_Tab:          return "TAB";
        case Qt::Key_Backspace:    return "BACKSPACE";
        case Qt::Key_Delete:       return "DELETE";
        case Qt::Key_Insert:       return "INSERT";
        case Qt::Key_Home:         return "HOME";
        case Qt::Key_End:          return "END";
        case Qt::Key_PageUp:       return "PAGEUP";
        case Qt::Key_PageDown:     return "PAGEDOWN";
        case Qt::Key_Left:         return "LEFT";
        case Qt::Key_Right:        return "RIGHT";
        case Qt::Key_Up:           return "UP";
        case Qt::Key_Down:         return "DOWN";
        case Qt::Key_BracketLeft:  return "[";
        case Qt::Key_BracketRight: return "]";
        case Qt::Key_Comma:        return ",";
        case Qt::Key_Period:       return ".";
        case Qt::Key_Slash:        return "/";
        case Qt::Key_Backslash:    return "\\";
        case Qt::Key_Equal:        return "=";
        case Qt::Key_Minus:        return "-";
        case Qt::Key_Semicolon:    return ";";
        case Qt::Key_Apostrophe:   return "'";
        case Qt::Key_QuoteLeft:    return "`";
        default: break;
    } // ah yes, INEFFICIENCY.
    // To do: maintain a static QHash<int, QString> for these keys.

    // Fallback: use the native text if it's a printable single character.
    if (!nativeText.isEmpty() && nativeText[0].isPrint())
        return nativeText.toUpper();

    return {};   // unknown key, doesn't capture/ignores, like my comments!
}
