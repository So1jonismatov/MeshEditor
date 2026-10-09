#pragma once

#include <QString>
#include <iostream>
#include <streambuf>
#include <string>

class LogConsole;

// Stream buffer redirector: tees output from std::cout / std::cerr to the
// LogConsole widget while preserving original stream output.
class LogConsoleBuffer : public std::streambuf
{
public:
    LogConsoleBuffer(LogConsole *console, std::ostream &stream,
                     const QString &prefix = QString());
    ~LogConsoleBuffer() override;

protected:
    int overflow(int ch) override;
    int sync() override;

private:
    void flushLine();

    LogConsole *m_console;
    std::ostream &m_stream;
    std::streambuf *m_original;
    std::string m_pending;
    QString m_prefix;
};
