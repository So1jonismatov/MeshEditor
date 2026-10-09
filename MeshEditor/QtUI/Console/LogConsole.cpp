#include "LogConsole.h"

#include <QFont>
#include <QFontDatabase>
#include <QMetaObject>
#include <QScrollBar>

#include <iostream>

LogConsole::LogConsole(QWidget *parent) : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setMaximumBlockCount(5000);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(9);
    setFont(mono);

    setStyleSheet("QPlainTextEdit { background-color: #14161a; color: #d4d7dc;"
                  " border: none; }");

    appendLine("[UI] Log console ready — std::cout and std::cerr are captured"
               " here.");

    m_coutBuffer =
        std::make_unique<RedirectBuffer>(this, std::cout, QString());
    m_cerrBuffer =
        std::make_unique<RedirectBuffer>(this, std::cerr, QString());
}

LogConsole::~LogConsole()
{
    // Restore the original stream buffers before the widget goes away.
    m_coutBuffer.reset();
    m_cerrBuffer.reset();
}

void LogConsole::appendLine(const QString &line)
{
    appendPlainText(line);
    QScrollBar *bar = verticalScrollBar();
    if (bar)
        bar->setValue(bar->maximum());
}

LogConsole::RedirectBuffer::RedirectBuffer(LogConsole *console,
                                           std::ostream &stream,
                                           const QString &prefix)
    : m_console(console), m_stream(stream), m_prefix(prefix)
{
    m_original = m_stream.rdbuf(this);
}

LogConsole::RedirectBuffer::~RedirectBuffer()
{
    flushLine();
    m_stream.rdbuf(m_original);
}

int LogConsole::RedirectBuffer::overflow(int ch)
{
    if (ch == EOF)
        return ch;

    if (m_original)
        m_original->sputc(static_cast<char>(ch));

    if (ch == '\n')
    {
        flushLine();
    }
    else
    {
        m_pending.push_back(static_cast<char>(ch));
    }
    return ch;
}

int LogConsole::RedirectBuffer::sync()
{
    if (m_original)
        m_original->pubsync();
    return 0;
}

void LogConsole::RedirectBuffer::flushLine()
{
    if (m_pending.empty())
        return;

    const QString line = m_prefix + QString::fromStdString(m_pending);
    m_pending.clear();

    // Queued so that logging from inside paint/input handling never touches
    // the widget re-entrantly.
    QMetaObject::invokeMethod(
        m_console, [console = m_console, line]() { console->appendLine(line); },
        Qt::QueuedConnection);
}
