#include "ui/main_window/main_widget.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QTreeWidget>
#include <QTreeWidgetItem>

void MainWidget::rebuildTreeFromModel()
{
    outlineTree->clear();
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(outlineTree, QStringList() << rootParam.name);
    rootItem->setData(0, Qt::UserRole, rootParam.name);
    createTreeItemsRecursively(rootItem, rootParam, QString("/") + rootParam.name);
    outlineTree->expandAll();
}

void MainWidget::createTreeItemsRecursively(QTreeWidgetItem* parentItem, const ParamMetadata& node, const QString& path)
{
    for (const auto& c : node.children) {
        QTreeWidgetItem* item = new QTreeWidgetItem(parentItem, QStringList() << c.name);
        const QString childPath = path + "/" + c.name;
        item->setData(0, Qt::UserRole, childPath);
        if (!c.children.isEmpty()) {
            createTreeItemsRecursively(item, c, childPath);
        }
    }
}

const ParamMetadata* MainWidget::findTypeNodeByPath(const QString& path) const
{
    QStringList parts = path.split('/', QString::SkipEmptyParts);
    if (parts.isEmpty()) return nullptr;
    if (parts.first() != rootParam.name) return nullptr;
    const ParamMetadata* cur = &rootParam;
    for (int i = 1; i < parts.size(); ++i) {
        bool ok = false;
        for (const auto& c : cur->children) {
            if (c.name == parts[i]) {
                cur = &c;
                ok = true;
                break;
            }
        }
        if (!ok) return nullptr;
    }
    return cur;
}

bool MainWidget::getParamByPath(const QString& path, ParamMetadata*& outParam)
{
    if (path.isEmpty()) return false;
    QStringList parts = path.split('/', QString::SkipEmptyParts);
    if (parts.isEmpty()) return false;
    if (parts.first() != rootParam.name) return false;
    ParamMetadata* cur = &rootParam;
    for (int i = 1; i < parts.size(); ++i) {
        bool found = false;
        for (int j = 0; j < cur->children.size(); ++j) {
            if (cur->children[j].name == parts[i]) {
                cur = &cur->children[j];
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    outParam = cur;
    return true;
}

bool MainWidget::getParentByPath(const QString& path, ParamMetadata*& outParent, int& childIndex)
{
    outParent = nullptr;
    childIndex = -1;
    if (path.isEmpty()) return false;
    QStringList parts = path.split('/', QString::SkipEmptyParts);
    if (parts.size() < 2) return false;
    if (parts.first() != rootParam.name) return false;
    ParamMetadata* cur = &rootParam;
    for (int i = 1; i < parts.size() - 1; ++i) {
        bool found = false;
        for (int j = 0; j < cur->children.size(); ++j) {
            if (cur->children[j].name == parts[i]) {
                cur = &cur->children[j];
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    outParent = cur;
    for (int j = 0; j < cur->children.size(); ++j) {
        if (cur->children[j].name == parts.last()) {
            childIndex = j;
            return true;
        }
    }
    return false;
}

void MainWidget::onTreeSelectionChanged()
{
    if (suppressTreeSelection) return;
    QTreeWidget* activeTree = onInstancesTab ? instancesTree : outlineTree;
    auto items = activeTree->selectedItems();
    if (items.isEmpty()) return;
    QTreeWidgetItem* item = items.first();
    const QString path = item->data(0, Qt::UserRole).toString();
    currentPath = path;
    ParamMetadata* p = nullptr;
    if (getParamByPath(path, p)) {
        fillFormFromParam(p);
        if (!onInstancesTab) {
            selectPropertyItem(path);
        } else {
            refreshInstanceCanvas();
        }
    }
}

void MainWidget::onOutlineContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = outlineTree->itemAt(pos);
    if (!item) return;
    const QString path = item->data(0, Qt::UserRole).toString();
    QMenu menu(this);
    QAction* actAddField  = menu.addAction(QString::fromUtf8(u8"\u6DFB\u52A0\u5B57\u6BB5"));
    QAction* actRename    = menu.addAction(QString::fromUtf8(u8"\u91CD\u547D\u540D"));
    QAction* actMoveUp    = menu.addAction(QString::fromUtf8(u8"\u4E0A\u79FB"));
    QAction* actMoveDown  = menu.addAction(QString::fromUtf8(u8"\u4E0B\u79FB"));
    QAction* actDelete    = menu.addAction(QString::fromUtf8(u8"\u5220\u9664"));
    QAction* chosen = menu.exec(outlineTree->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    ParamMetadata* p = nullptr;
    if (!getParamByPath(path, p)) return;

    if (chosen == actAddField) {
        if (!canHaveChildren(p->type)) {
            QMessageBox::warning(this, QString::fromUtf8(u8"\u64CD\u4F5C\u65E0\u6548"), QString::fromUtf8(u8"\u8BE5\u7C7B\u578B\u4E0D\u80FD\u6DFB\u52A0\u5B50\u9879"));
            return;
        }
        ParamMetadata child;
        child.name = "newField";
        child.type = ParamType::UINT16;
        p->children.append(child);
        rebuildTreeFromModel();
        updateValidationStatus();
        refreshCenterCanvas();
    } else if (chosen == actRename) {
        bool ok = false;
        QString newName = QInputDialog::getText(this, QString::fromUtf8(u8"\u91CD\u547D\u540D"), QString::fromUtf8(u8"\u8F93\u5165\u65B0\u540D\u79F0"), QLineEdit::Normal, p->name, &ok);
        if (ok && !newName.isEmpty()) {
            QStringList parts = path.split('/', QString::SkipEmptyParts);
            parts.last() = newName;
            const QString newPath = QString("/") + parts.join("/");
            p->name = newName;
            rebuildTreeFromModel();
            refreshCenterCanvas();
            updateValidationStatus();
            currentPath = newPath;
            QList<QTreeWidgetItem*> items = outlineTree->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
            for (auto* it : items) {
                if (it->data(0, Qt::UserRole).toString() == newPath) {
                    outlineTree->setCurrentItem(it);
                    break;
                }
            }
        }
    } else if (chosen == actMoveUp || chosen == actMoveDown) {
        ParamMetadata* parent = nullptr;
        int idx = -1;
        if (!getParentByPath(path, parent, idx) || !parent) return;
        int newIdx = idx + (chosen == actMoveUp ? -1 : 1);
        if (newIdx < 0 || newIdx >= parent->children.size()) return;
        ParamMetadata tmp = parent->children[idx];
        parent->children[idx] = parent->children[newIdx];
        parent->children[newIdx] = tmp;
        rebuildTreeFromModel();
        updateValidationStatus();
        refreshCenterCanvas();
    } else if (chosen == actDelete) {
        auto ret = QMessageBox::question(this, QString::fromUtf8(u8"\u786E\u8BA4\u5220\u9664"), QString::fromUtf8(u8"\u786E\u5B9A\u5220\u9664\u8BE5\u9879\u5417\uFF1F"));
        if (ret != QMessageBox::Yes) return;
        ParamMetadata* parent = nullptr;
        int idx = -1;
        if (getParentByPath(path, parent, idx) && parent && idx >= 0) {
            parent->children.remove(idx);
            rebuildTreeFromModel();
            updateValidationStatus();
            refreshCenterCanvas();
        }
    }
}

void MainWidget::onOutlineRowsMoved(const QModelIndex& srcParent, int start, int end, const QModelIndex& dstParent, int dstRow)
{
    Q_UNUSED(end);
    Q_UNUSED(dstParent);
    QTreeWidgetItem* parentItem = nullptr;
    if (srcParent.isValid()) parentItem = outlineTree->topLevelItem(0);
    if (!parentItem) parentItem = outlineTree->invisibleRootItem()->child(0);
    QString parentPath = parentItem ? parentItem->data(0, Qt::UserRole).toString() : QString();
    ParamMetadata* parentParam = nullptr;
    if (!getParamByPath(parentPath, parentParam)) return;
    if (!parentParam) return;
    if (start < 0 || start >= parentParam->children.size()) return;
    int insertPos = dstRow;
    if (insertPos > parentParam->children.size()) insertPos = parentParam->children.size();
    if (insertPos == start) return;
    auto moved = parentParam->children.takeAt(start);
    if (insertPos > start) --insertPos;
    parentParam->children.insert(insertPos, moved);
    rebuildTreeFromModel();
    refreshCenterCanvas();
    updateValidationStatus();
}

void MainWidget::onDeleteSelected()
{
    auto items = outlineTree->selectedItems();
    if (items.isEmpty()) return;
    const QString path = items.first()->data(0, Qt::UserRole).toString();
    auto ret = QMessageBox::question(this, QString::fromUtf8(u8"\u786E\u8BA4\u5220\u9664"), QString::fromUtf8(u8"\u786E\u5B9A\u5220\u9664\u8BE5\u9879\u5417\uFF1F"));
    if (ret != QMessageBox::Yes) return;
    ParamMetadata* parent = nullptr;
    int idx = -1;
    if (getParentByPath(path, parent, idx) && parent && idx >= 0) {
        parent->children.remove(idx);
        rebuildTreeFromModel();
        refreshCenterCanvas();
        updateValidationStatus();
    }
}

void MainWidget::onRenameSelected()
{
    auto items = outlineTree->selectedItems();
    if (items.isEmpty()) return;
    const QString path = items.first()->data(0, Qt::UserRole).toString();
    ParamMetadata* p = nullptr;
    if (!getParamByPath(path, p)) return;
    bool ok = false;
    QString newName = QInputDialog::getText(this, QString::fromUtf8(u8"重命名"), QString::fromUtf8(u8"输入新名称"), QLineEdit::Normal, p->name, &ok);
    if (ok && !newName.isEmpty()) {
        QStringList parts = path.split('/', QString::SkipEmptyParts);
        parts.last() = newName;
        const QString newPath = QString("/") + parts.join("/");
        p->name = newName;
        rebuildTreeFromModel();
        refreshCenterCanvas();
        updateValidationStatus();
        currentPath = newPath;
        QList<QTreeWidgetItem*> list = outlineTree->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
        for (auto* it : list) {
            if (it->data(0, Qt::UserRole).toString() == newPath) {
                outlineTree->setCurrentItem(it);
                break;
            }
        }
    }
}
