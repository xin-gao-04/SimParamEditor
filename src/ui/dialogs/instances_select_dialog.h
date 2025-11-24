#ifndef INSTANCES_SELECT_DIALOG_H
#define INSTANCES_SELECT_DIALOG_H

#include <QDialog>
#include <QStringList>

class QListWidget;

// 弹窗：供用户批量选择要生成代码的实例
class InstancesSelectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit InstancesSelectDialog(const QStringList& instanceNames, QWidget* parent = nullptr);
    QStringList selected() const;

private:
    QListWidget* listWidget = nullptr;
};

#endif // INSTANCES_SELECT_DIALOG_H
