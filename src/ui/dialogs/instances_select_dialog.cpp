#include "ui/dialogs/instances_select_dialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

InstancesSelectDialog::InstancesSelectDialog(const QStringList& instanceNames, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8(u8"选择要生成的实例"));
    auto* v = new QVBoxLayout(this);
    listWidget = new QListWidget(this);
    for (const auto& name : instanceNames) {
        auto* item = new QListWidgetItem(name, listWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    v->addWidget(listWidget);

    auto* batchRow = new QHBoxLayout();
    auto* allBtn = new QPushButton(QString::fromUtf8(u8"全选"), this);
    auto* noneBtn = new QPushButton(QString::fromUtf8(u8"全不选"), this);
    batchRow->addWidget(allBtn);
    batchRow->addWidget(noneBtn);
    batchRow->addStretch(1);
    v->addLayout(batchRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(buttons);

    connect(allBtn, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < listWidget->count(); ++i) listWidget->item(i)->setCheckState(Qt::Checked);
    });
    connect(noneBtn, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < listWidget->count(); ++i) listWidget->item(i)->setCheckState(Qt::Unchecked);
    });

    setMinimumSize(360, 400);
}

QStringList InstancesSelectDialog::selected() const
{
    QStringList out;
    for (int i = 0; i < listWidget->count(); ++i) {
        auto* item = listWidget->item(i);
        if (item && item->checkState() == Qt::Checked) out << item->text();
    }
    return out;
}
