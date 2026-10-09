#include "LogConsoleBuffer.h"
#include "LogConsole.h"
#include <QMetaObject>

LogConsoleBuffer::LogConsoleBuffer(LogConsole *console, std::ostream &stream,
                                   const QString &prefix)
    : m_console(console), m_stream(stream), m_prefix(prefix)
{
    m_original = m_stream.rdbuf(this);
}

LogConsoleBuffer::~LogConsoleBuffer()
{
    flushLine();
    m_stream.rdbuf(m_original);
}

int LogConsoleBuffer::overflow(int ch)
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

int LogConsoleBuffer::sync()
{
    if (m_original)
        m_original->pubsync();
    return 0;
}

void LogConsoleBuffer::flushLine()
{
    if (m_pending.empty())
        return;

    const QString line = m_prefix + QString::fromStdString(m_pending);
    m_pending.clear();

    QMetaObject::invokeMethod(
        m_console, [console = m_console, line]() { console->appendLine(line); },
        Qt::QueuedConnection);
}
