#pragma once
// ---------------------------------------------------------------------------
// MonitorWindow.h - the "Server Monitoring GUI" from the use cases
// (UC 7.2, 12-15) and the "ServerGUI" class in the class diagram.
//
// It's a plain Qt Widgets window that opens alongside the server's terminal.
// The terminal keeps showing the log; this window is for the admin:
//
//   [View Logged-In Users]  [View Registered User Count]
//   [View Locked-Out Users] [View Connected User Count]
//   ------------------------------------------------------
//   heading, e.g. "Logged-in users: 2"
//   list of results (or the "No ... currently ..." message)
//   [Unlock Selected Account]    (only in the locked-out view)
//   ------------------------------------------------------
//   Broadcast: [______________] [Send]
//
// HOW IT STAYS UP TO DATE: the Server emits stateChanged() whenever a client
// connects, disconnects, logs in/out, registers, or gets locked/unlocked.
// That signal is connected to refresh(), so the current view redraws itself.
// A 5-second timer also refreshes, so a lock that expires on its own
// disappears from the locked-out list without anyone clicking.
//
// THREADING: Qt widgets live on the main thread, the same thread the Server
// runs on, so the window can call Server methods directly. No locks.
//
// Closing this window shuts the server down (Qt quits when the last window
// closes), the same as typing "exit" in the terminal.
// ---------------------------------------------------------------------------

#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class Server;

class MonitorWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorWindow(Server& server, QWidget* parent = nullptr);

private:
    // Which list the window is showing. Each one matches a use case.
    enum class View {
        LoggedIn,     // UC 12 / issue #65
        Registered,   // UC 13 / issue #74
        Locked,       // UC 14 / issue #73 (+ unlock, UC 7.2)
        Connected     // UC 15 / issue #72
    };

    void showView(View view);   // switch views (button click)
    void refresh();             // redraw the current view from live data
    void unlockSelected();      // Unlock button
    void sendBroadcast();       // Send button / Enter in the broadcast box

    // Fills the list for each view.
    void fillLoggedIn();
    void fillRegistered();
    void fillLocked();
    void fillConnected();

    // Shows a single grey message line in the list (empty results, errors).
    void showMessage(const QString& text);

    Server&      m_server;
    View         m_view = View::Connected;

    QLabel*      m_heading      = nullptr;   // "Logged-in users: 2"
    QListWidget* m_list         = nullptr;   // the results
    QPushButton* m_unlockButton = nullptr;   // only visible in the Locked view
    QLabel*      m_status       = nullptr;   // feedback line at the bottom
    QLineEdit*   m_broadcastBox = nullptr;
};
