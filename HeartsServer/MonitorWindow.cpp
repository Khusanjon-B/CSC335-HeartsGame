// ---------------------------------------------------------------------------
// MonitorWindow.cpp - see MonitorWindow.h.
//
// Text shown for empty results and database errors is copied from the use
// cases / test cases so the manual tests can check it word for word:
//   "No users currently logged in"                        (UC 12, TC 13.1)
//   "No accounts currently locked"                        (UC 14, TC 15.1)
//   "No accounts currently connected"                     (UC 15, TC 16.1)
//   "An error occurred while trying to reach the database." (UC 7.2, 12-14)
// ---------------------------------------------------------------------------

#include "MonitorWindow.h"
#include "Log.h"
#include "Server.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

static const QString kDatabaseError = "An error occurred while trying to reach the database.";

MonitorWindow::MonitorWindow(Server& server, QWidget* parent)
    : QWidget(parent)
    , m_server(server)
{
    setWindowTitle("Hearts Server Monitor");
    resize(520, 460);

    // ---- the four view buttons, in a 2x2 grid ----
    auto* loggedInBtn   = new QPushButton("View Logged-In Users");
    auto* registeredBtn = new QPushButton("View Registered User Count");
    auto* lockedBtn     = new QPushButton("View Locked-Out Users");
    auto* connectedBtn  = new QPushButton("View Connected User Count");

    auto* buttons = new QGridLayout;
    buttons->addWidget(loggedInBtn,   0, 0);
    buttons->addWidget(registeredBtn, 0, 1);
    buttons->addWidget(lockedBtn,     1, 0);
    buttons->addWidget(connectedBtn,  1, 1);

    // Each button just switches the view. Lambdas let us pass which view.
    connect(loggedInBtn,   &QPushButton::clicked, this, [this] { showView(View::LoggedIn); });
    connect(registeredBtn, &QPushButton::clicked, this, [this] { showView(View::Registered); });
    connect(lockedBtn,     &QPushButton::clicked, this, [this] { showView(View::Locked); });
    connect(connectedBtn,  &QPushButton::clicked, this, [this] { showView(View::Connected); });

    // ---- heading + results list + unlock button ----
    m_heading = new QLabel;
    QFont headingFont = m_heading->font();
    headingFont.setBold(true);
    headingFont.setPointSize(headingFont.pointSize() + 2);
    m_heading->setFont(headingFont);

    m_list = new QListWidget;

    m_unlockButton = new QPushButton("Unlock Selected Account");
    connect(m_unlockButton, &QPushButton::clicked, this, &MonitorWindow::unlockSelected);

    // ---- broadcast box (same as typing in the server terminal) ----
    m_broadcastBox = new QLineEdit;
    m_broadcastBox->setPlaceholderText("Message to every connected client");
    auto* sendBtn = new QPushButton("Send");
    connect(sendBtn, &QPushButton::clicked, this, &MonitorWindow::sendBroadcast);
    connect(m_broadcastBox, &QLineEdit::returnPressed, this, &MonitorWindow::sendBroadcast);

    auto* broadcastRow = new QHBoxLayout;
    broadcastRow->addWidget(new QLabel("Broadcast:"));
    broadcastRow->addWidget(m_broadcastBox, 1);   // 1 = stretch to fill the row
    broadcastRow->addWidget(sendBtn);

    m_status = new QLabel;
    m_status->setStyleSheet("color: gray;");

    // ---- stack everything vertically ----
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(buttons);
    layout->addWidget(m_heading);
    layout->addWidget(m_list, 1);                 // the list takes the spare space
    layout->addWidget(m_unlockButton);
    layout->addLayout(broadcastRow);
    layout->addWidget(m_status);

    // ---- keep the view live ----
    // Redraw whenever the server says something changed...
    connect(&m_server, &Server::stateChanged, this, &MonitorWindow::refresh);
    // ...and every 5 seconds, so expired locks drop off the list by themselves.
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MonitorWindow::refresh);
    timer->start(5000);

    showView(View::Connected);
}

void MonitorWindow::showView(View view)
{
    m_view = view;
    m_status->clear();
    refresh();
}

void MonitorWindow::refresh()
{
    m_list->clear();
    // The unlock button only makes sense next to the locked-out list.
    m_unlockButton->setVisible(m_view == View::Locked);

    switch (m_view) {
    case View::LoggedIn:   fillLoggedIn();   break;
    case View::Registered: fillRegistered(); break;
    case View::Locked:     fillLocked();     break;
    case View::Connected:  fillConnected();  break;
    }
}

// ---- UC 12: View Logged-In Users ----
void MonitorWindow::fillLoggedIn()
{
    const auto users = m_server.viewLoggedInUsers();
    m_heading->setText(QString("Logged-in users: %1").arg(users.size()));
    if (users.empty())
        return showMessage("No users currently logged in");

    for (const auto& u : users)
        m_list->addItem(u.username + "    (" + u.ip + ")");
}

// ---- UC 13: View Registered User Count ----
void MonitorWindow::fillRegistered()
{
    const std::optional<int> count = m_server.viewRegisteredUsers();
    if (!count) {
        m_heading->setText("Registered users: ?");
        return showMessage(kDatabaseError + " Click the button again to retry.");
    }
    m_heading->setText(QString("Registered users: %1").arg(*count));
    showMessage(QString("%1 account(s) in the database.").arg(*count));
}

// ---- UC 14: View Locked-Out Users ----
void MonitorWindow::fillLocked()
{
    const auto locked = m_server.viewLockedAccounts();
    if (!locked) {
        m_heading->setText("Locked-out users: ?");
        return showMessage(kDatabaseError + " Click the button again to retry.");
    }

    m_heading->setText(QString("Locked-out users: %1").arg(locked->size()));
    m_unlockButton->setEnabled(!locked->empty());
    if (locked->empty())
        return showMessage("No accounts currently locked");

    const QDateTime now = QDateTime::currentDateTime();
    for (const LockedAccount& acc : *locked) {
        const qint64 secs = now.secsTo(acc.lockedUntil);
        const QString left = QString("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QChar('0'));
        auto* item = new QListWidgetItem(acc.username + "    (unlocks in " + left + ")");
        item->setData(Qt::UserRole, acc.username);   // remember the plain username for Unlock
        m_list->addItem(item);
    }
    m_list->setCurrentRow(0);
}

// ---- UC 15: View Connected User Count ----
void MonitorWindow::fillConnected()
{
    const auto clients = m_server.viewConnectedUsers();
    m_heading->setText(QString("Connected clients: %1").arg(clients.size()));
    if (clients.empty())
        return showMessage("No accounts currently connected");

    for (const auto& c : clients)
        m_list->addItem(c.ip + "    " + (c.loggedIn ? c.username : QString("(not logged in)")));
}

// ---- UC 7.2: Admin unlocks account ----
void MonitorWindow::unlockSelected()
{
    QListWidgetItem* item = m_list->currentItem();
    const QString username = item ? item->data(Qt::UserRole).toString() : QString();
    if (username.isEmpty()) {
        m_status->setText("Select a locked account first.");
        return;
    }

    if (m_server.unlockAccount(username))
        m_status->setText("Unlocked \"" + username + "\". They can sign in again.");
    else
        m_status->setText(kDatabaseError);
    refresh();   // the server also emits stateChanged, but this covers the error case
}

void MonitorWindow::sendBroadcast()
{
    const QString text = m_broadcastBox->text().trimmed();
    if (text.isEmpty())
        return;
    m_server.broadcast(text);
    m_broadcastBox->clear();
    m_status->setText("Broadcast sent: " + text);
}

void MonitorWindow::showMessage(const QString& text)
{
    auto* item = new QListWidgetItem(text);
    item->setFlags(Qt::NoItemFlags);           // grey, can't be selected
    m_list->addItem(item);
}
