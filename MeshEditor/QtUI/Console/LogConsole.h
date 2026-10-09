#pragma once

#include <QPlainTextEdit>

#include <memory>
#include <streambuf>
#include <string>

// Integrated "terminal" panel: captures std::cout / std::cerr (which all
// existing operators log to) and shows them in a read-only console view.
// Output is still forwarded to the original streams.
class LogConsole : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit LogConsole(QWidget *parent = nullptr);
    ~LogConsole() override;

public slots:
    void appendLine(const QString &line);

private:
    class RedirectBuffer : public std::streambuf
    {
    public:
        RedirectBuffer(LogConsole *console, std::ostream &stream,
                       const QString &prefix);
        ~RedirectBuffer() override;

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

    std::unique_ptr<RedirectBuffer> m_coutBuffer;
    std::unique_ptr<RedirectBuffer> m_cerrBuffer;
};
