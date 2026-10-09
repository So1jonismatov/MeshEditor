#pragma once

#include <QDialog>
#include <vector>
#include <utility>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;

class ShortcutsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutsDialog(QWidget *parent = nullptr);

private slots:
    void filterShortcuts(const QString &query);

private:
    void addCategory(const QString &categoryName,
                     const std::vector<std::pair<QString, QString>> &shortcuts);

    QLineEdit *m_searchEdit = nullptr;
    QTreeWidget *m_treeWidget = nullptr;
};
