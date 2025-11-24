#include "ui/main_window/main_widget.h"

#include "core/type_manager.h"

#include <QComboBox>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

bool MainWidget::findInstanceByName(const QString& name, InstanceMetadata*& outInst)
{
    for (auto& in : instances) {
        if (in.name == name) {
            outInst = &in;
            return true;
        }
    }
    outInst = nullptr;
    return false;
}

static void renderFieldsRecursive(QVBoxLayout* layout, const ParamMetadata& typeNode, InstanceMetadata& inst, const QStringList& relPath, QObject* receiver)
{
    const ParamMetadata* src = &typeNode;
    if (!typeNode.typeName.isEmpty()) {
        const ParamMetadata* typeDef = TypeManager::instance().getType(typeNode.typeName);
        if (typeDef) src = typeDef;
    }

    for (const auto& c : src->children) {
        QStringList childPath = relPath;
        childPath << c.name;
        const QString flatKey = childPath.join("/");

        if (c.type == ParamType::STRUCT) {
            QWidget* group = new QWidget;
            auto* v = new QVBoxLayout(group);
            v->setContentsMargins(8, 8, 8, 8);
            QLabel* title = new QLabel("[" + c.name + "]", group);
            v->addWidget(title);
            renderFieldsRecursive(v, c, inst, childPath, receiver);
            layout->addWidget(group);
        } else {
            QWidget* row = new QWidget;
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0, 4, 0, 4);
            QLabel* name = new QLabel(c.name, row);
            name->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            name->setMinimumWidth(180);
            name->setMaximumWidth(180);
            h->addWidget(name);

            QVariant val = inst.values.value(flatKey);
            if (!val.isValid()) val = c.defaultValue;

            if (c.type == ParamType::ENUM) {
                QComboBox* combo = new QComboBox(row);
                const ParamMetadata* enumDef = &c;
                if (!c.typeName.isEmpty()) {
                    const ParamMetadata* t = TypeManager::instance().getType(c.typeName);
                    if (t) enumDef = t;
                }
                combo->addItems(enumDef->enumItems);
                combo->setCurrentText(val.toString());
                combo->setProperty("instRelPath", flatKey);
                QObject::connect(combo, SIGNAL(currentIndexChanged(int)), receiver, SLOT(onInstanceFieldComboChanged(int)));
                h->addWidget(combo);
            } else if (c.type == ParamType::FLOAT || c.type == ParamType::DOUBLE) {
                QLineEdit* editor = new QLineEdit(row);
                editor->setValidator(new QDoubleValidator(editor));
                editor->setProperty("instRelPath", flatKey);
                editor->setText(val.toString());
                QObject::connect(editor, SIGNAL(editingFinished()), receiver, SLOT(onInstanceEditorEdited()));
                h->addWidget(editor);
            } else if (isNumericType(c.type)) {
                QLineEdit* editor = new QLineEdit(row);
                editor->setValidator(new QIntValidator(editor));
                editor->setProperty("instRelPath", flatKey);
                editor->setText(val.toString());
                QObject::connect(editor, SIGNAL(editingFinished()), receiver, SLOT(onInstanceEditorEdited()));
                h->addWidget(editor);
            } else {
                QLineEdit* editor = new QLineEdit(row);
                editor->setProperty("instRelPath", flatKey);
                editor->setText(val.toString());
                QObject::connect(editor, SIGNAL(editingFinished()), receiver, SLOT(onInstanceEditorEdited()));
                h->addWidget(editor);
            }
            layout->addWidget(row);
        }
    }
}

QWidget* MainWidget::renderInstanceEditor(const InstanceMetadata& instConst, const ParamMetadata& typeNode)
{
    InstanceMetadata* inst = nullptr;
    if (!findInstanceByName(instConst.name, inst)) return new QWidget;
    QWidget* panel = new QWidget;
    auto* v = new QVBoxLayout(panel);
    v->setContentsMargins(12, 12, 12, 12);
    QLabel* head = new QLabel(QStringLiteral("编辑实例: ") + inst->name + "  (" + inst->typePath + ")", panel);
    v->addWidget(head);
    renderFieldsRecursive(v, typeNode, *inst, QStringList(), this);
    v->addStretch(1);
    return panel;
}

void MainWidget::refreshInstanceCanvas()
{
    if (!instanceLayout || !centerStack) return;
    if (leftTabs && leftTabs->currentIndex() != 1) return;
    centerStack->setCurrentWidget(instanceScrollArea);

    QLayoutItem* item;
    while ((item = instanceLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    QString instName;
    auto sel = instancesTree->selectedItems();
    if (!sel.isEmpty()) instName = sel.first()->text(0);
    else if (!instances.isEmpty()) instName = instances.first().name;
    currentInstanceName = instName;

    InstanceMetadata* inst = nullptr;
    if (!instName.isEmpty() && findInstanceByName(instName, inst)) {
        const ParamMetadata* tn = findTypeNodeByPath(inst->typePath);
        if (tn) instanceLayout->addWidget(renderInstanceEditor(*inst, *tn));
    }
    instanceLayout->addStretch(1);
}

void MainWidget::onInstanceEditorEdited()
{
    if (!sender()) return;
    QLineEdit* editor = qobject_cast<QLineEdit*>(sender());
    if (!editor) return;
    const QString rel = editor->property("instRelPath").toString();
    InstanceMetadata* inst = nullptr;
    if (!findInstanceByName(currentInstanceName, inst)) return;
    if (rel.isEmpty()) return;
    inst->values[rel] = editor->text();
}

void MainWidget::onInstanceFieldComboChanged(int)
{
    if (!sender()) return;
    QComboBox* combo = qobject_cast<QComboBox*>(sender());
    if (!combo) return;
    const QString rel = combo->property("instRelPath").toString();
    InstanceMetadata* inst = nullptr;
    if (!findInstanceByName(currentInstanceName, inst)) return;
    if (rel.isEmpty()) return;
    inst->values[rel] = combo->currentText();
}

static void collectLeafFields(const ParamMetadata& typeNode, QStringList& out)
{
    const ParamMetadata* src = &typeNode;
    if (!typeNode.typeName.isEmpty()) {
        const ParamMetadata* def = TypeManager::instance().getType(typeNode.typeName);
        if (def) src = def;
    }
    for (const auto& c : src->children) {
        if (c.type == ParamType::STRUCT) collectLeafFields(c, out);
        else out << c.name;
    }
}

bool MainWidget::editInstanceValues(InstanceMetadata& inst)
{
    const ParamMetadata* typeNode = findTypeNodeByPath(inst.typePath);
    if (!typeNode) return false;
    QStringList leafs;
    collectLeafFields(*typeNode, leafs);
    for (const auto& f : leafs) {
        const QString cur = inst.values.value(f).toString();
        bool ok = false;
        QString v = QInputDialog::getText(this, QStringLiteral("编辑实例值"), f + QStringLiteral(" 的值"), QLineEdit::Normal, cur, &ok);
        if (!ok) continue;
        inst.values[f] = v;
    }
    return true;
}

void MainWidget::rebuildInstancesTree()
{
    if (!instancesTree) return;
    instancesTree->clear();
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(instancesTree, QStringList() << QStringLiteral("实例"));
    for (const auto& inst : instances) {
        QTreeWidgetItem* item = new QTreeWidgetItem(rootItem, QStringList() << inst.name);
        item->setData(0, Qt::UserRole, inst.typePath);
    }
    instancesTree->expandAll();
}

void MainWidget::onInstancesContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = instancesTree->itemAt(pos);
    QMenu menu(this);
    QAction* actAdd = menu.addAction(QStringLiteral("新建实例"));
    QAction* actEdit = menu.addAction(QStringLiteral("编辑值"));
    QAction* actRename = nullptr;
    QAction* actDelete = nullptr;
    if (item) {
        actRename = menu.addAction(QStringLiteral("重命名"));
        actDelete = menu.addAction(QStringLiteral("删除"));
    }
    QAction* chosen = menu.exec(instancesTree->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actAdd) {
        QStringList types;
        for (const auto& c : rootParam.children) if (c.type == ParamType::STRUCT) types << c.name;
        bool ok = false;
        QString typeName = QInputDialog::getItem(this, QStringLiteral("选择类型"), QStringLiteral("从模板选择结构体"), types, 0, false, &ok);
        if (!ok || typeName.isEmpty()) return;
        bool ok2 = false;
        QString instName = QInputDialog::getText(this, QStringLiteral("新建实例"), QStringLiteral("实例名称"), QLineEdit::Normal, "NewInstance", &ok2);
        if (!ok2 || instName.isEmpty()) return;
        InstanceMetadata in;
        in.name = instName;
        in.typePath = "/" + rootParam.name + "/" + typeName;
        instances.append(in);
        rebuildInstancesTree();
    } else if (chosen == actEdit) {
        if (!item) return;
        for (auto& in : instances) {
            if (in.name == item->text(0)) {
                editInstanceValues(in);
                break;
            }
        }
        rebuildInstancesTree();
    } else if (actRename && chosen == actRename) {
        bool ok = false;
        QString newName = QInputDialog::getText(this, QStringLiteral("重命名"), QStringLiteral("新名称"), QLineEdit::Normal, item->text(0), &ok);
        if (!ok || newName.isEmpty()) return;
        for (auto& in : instances) if (in.name == item->text(0)) { in.name = newName; break; }
        rebuildInstancesTree();
    } else if (actDelete && chosen == actDelete) {
        auto ret = QMessageBox::question(this, QStringLiteral("确认删除"), QStringLiteral("确定删除该实例吗？"));
        if (ret != QMessageBox::Yes) return;
        for (int i = 0; i < instances.size(); ++i) if (instances[i].name == item->text(0)) { instances.remove(i); break; }
        rebuildInstancesTree();
    }
}
