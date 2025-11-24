#include "ui/main_window/main_widget.h"

#include "core/type_manager.h"
#include "core/validation.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QGroupBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegExp>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTextEdit>

void MainWidget::refreshCenterCanvas()
{
    if (!propertyBrowser || !centerStack) return;
    centerStack->setCurrentWidget(propertyBrowser);
    populatePropertyBrowser();
    if (!currentPath.isEmpty()) {
        selectPropertyItem(currentPath);
    }
}

void MainWidget::populatePropertyBrowser()
{
    if (!propertyBrowser) return;
    propertyBrowser->clear();

    QMap<QString, ValidationIssue::Level> levelMap;
    QMap<QString, QString> messageMap;
    {
        auto report = validateProject(rootParam);
        for (const auto& issue : report.issues) {
            const QString& p = issue.path;
            auto it = levelMap.find(p);
            if (it == levelMap.end()) {
                levelMap.insert(p, issue.level);
                messageMap.insert(p, issue.message);
            } else {
                if (it.value() == ValidationIssue::Warning && issue.level == ValidationIssue::Error) {
                    it.value() = issue.level;
                    messageMap[p] = issue.message;
                }
            }
        }
    }

    ParamMetadata* base = nullptr;
    QString basePath;
    if (!currentPath.isEmpty() && getParamByPath(currentPath, base) && base && base->type == ParamType::STRUCT) {
        basePath = currentPath;
    } else {
        base = &rootParam;
        basePath = "/" + rootParam.name;
    }
    if (!base) return;

    struct Local {
        static void collect(QTreeWidget* view,
                            const ParamMetadata& node,
                            const QString& relPath,
                            const QString& fullPath,
                            const QMap<QString, ValidationIssue::Level>& levelMap,
                            const QMap<QString, QString>& messageMap)
        {
            const ParamMetadata* src = &node;
            if (!node.typeName.isEmpty()) {
                const ParamMetadata* def = TypeManager::instance().getType(node.typeName);
                if (def) src = def;
            }
            for (const auto& c : src->children) {
                const QString childRel  = relPath.isEmpty() ? c.name : (relPath + "/" + c.name);
                const QString childFull = fullPath + "/" + c.name;
                if (c.type == ParamType::STRUCT) {
                    collect(view, c, childRel, childFull, levelMap, messageMap);
                } else {
                    QTreeWidgetItem* item = new QTreeWidgetItem(view);
                    item->setText(0, c.name);
                    item->setText(1, childRel);
                    item->setText(2, paramTypeToString(c.type));
                    item->setText(3, c.unit);
                    if (c.defaultValue.isValid()) {
                        item->setText(4, c.defaultValue.toString());
                    } else if (c.type == ParamType::CHAR_ARRAY && c.arraySize > 0) {
                        item->setText(4, QString("char[%1]").arg(c.arraySize));
                    } else {
                        item->setText(4, "-");
                    }
                    item->setText(5, c.typeName.isEmpty() ? "-" : c.typeName);

                    QString status;
                    auto it = levelMap.find(childFull);
                    if (it != levelMap.end()) {
                        const bool isErr = (it.value() == ValidationIssue::Error);
                        status = isErr ? QString::fromUtf8(u8"[错误] ") : QString::fromUtf8(u8"[警告] ");
                        status += messageMap.value(childFull);
                    } else {
                        status = QString::fromUtf8(u8"通过");
                    }
                    item->setText(6, status);
                    item->setData(0, Qt::UserRole, childFull);
                }
            }
        }
    };

    Local::collect(propertyBrowser, *base, QString(), basePath, levelMap, messageMap);
}

void MainWidget::selectPropertyItem(const QString& path)
{
    if (!propertyBrowser) return;
    suppressPropertySelection = true;
    QList<QTreeWidgetItem*> queue;
    for (int i = 0; i < propertyBrowser->topLevelItemCount(); ++i) {
        queue.append(propertyBrowser->topLevelItem(i));
    }

    QTreeWidgetItem* target = nullptr;
    while (!queue.isEmpty()) {
        QTreeWidgetItem* item = queue.takeFirst();
        if (item->data(0, Qt::UserRole).toString() == path) {
            target = item;
            break;
        }
        for (int i = 0; i < item->childCount(); ++i) {
            queue.append(item->child(i));
        }
    }

    if (target) {
        propertyBrowser->setCurrentItem(target);
        propertyBrowser->scrollToItem(target, QAbstractItemView::PositionAtCenter);
    }
    suppressPropertySelection = false;
}

void MainWidget::normalizeEnumWorkingValues()
{
    while (enumWorkingValues.size() < enumWorkingItems.size()) {
        int next = enumWorkingValues.isEmpty() ? 0 : enumWorkingValues.last() + 1;
        enumWorkingValues.append(next);
    }
    if (enumWorkingValues.size() > enumWorkingItems.size()) {
        enumWorkingValues.resize(enumWorkingItems.size());
    }
}

void MainWidget::syncEnumEditor()
{
    if (!enumTable) return;
    normalizeEnumWorkingValues();
    suppressEnumTableSignal = true;
    enumTable->setRowCount(enumWorkingItems.size());
    for (int i = 0; i < enumWorkingItems.size(); ++i) {
        auto* nameItem = new QTableWidgetItem(enumWorkingItems.at(i));
        enumTable->setItem(i, 0, nameItem);
        auto* valueItem = new QTableWidgetItem(QString::number(enumWorkingValues.value(i, i)));
        enumTable->setItem(i, 1, valueItem);
    }
    suppressEnumTableSignal = false;
    updateEnumButtonsState();
    ensureEnumDefaultValid();
    refreshEnumDefaultCombo();
}

void MainWidget::updateEnumButtonsState()
{
    const bool editable = enumTable && enumTable->isEnabled();
    if (enumAddRowBtn) enumAddRowBtn->setEnabled(editable);
    if (enumRemoveRowBtn) {
        bool hasSelection = enumTable && enumTable->currentRow() >= 0;
        enumRemoveRowBtn->setEnabled(editable && hasSelection);
    }
}

void MainWidget::refreshEnumDefaultCombo()
{
    if (!enumDefaultCombo) return;
    enumDefaultCombo->clear();
    for (int i = 0; i < enumWorkingItems.size(); ++i) {
        const QString name = enumWorkingItems.at(i);
        const int val = enumWorkingValues.value(i, i);
        enumDefaultCombo->addItem(QString("%1 (%2)").arg(name).arg(val), name);
    }
    const bool hasItems = !enumWorkingItems.isEmpty();
    enumDefaultCombo->setVisible(hasItems);
    if (!hasItems) {
        enumWorkingDefault.clear();
        return;
    }
    int idx = enumDefaultCombo->findData(enumWorkingDefault);
    if (idx < 0) idx = 0;
    enumDefaultCombo->setCurrentIndex(idx);
    enumWorkingDefault = enumDefaultCombo->itemData(idx).toString();
}

void MainWidget::ensureEnumDefaultValid()
{
    if (enumWorkingItems.isEmpty()) {
        enumWorkingDefault.clear();
        return;
    }
    if (!enumWorkingItems.contains(enumWorkingDefault)) {
        enumWorkingDefault = enumWorkingItems.first();
    }
}

void MainWidget::onEnumTableCellChanged(int row, int column)
{
    if (suppressEnumTableSignal) return;
    if (row < 0 || row >= enumWorkingItems.size()) return;
    if (column == 0) {
        QString name = enumTable->item(row, column) ? enumTable->item(row, column)->text().trimmed() : QString();
        if (name.isEmpty()) name = QString("Item%1").arg(row + 1);
        enumWorkingItems[row] = name;
        if (enumWorkingDefault.isEmpty()) enumWorkingDefault = name;
        refreshEnumDefaultCombo();
    } else if (column == 1) {
        bool ok = false;
        int val = enumTable->item(row, column) ? enumTable->item(row, column)->text().toInt(&ok) : 0;
        if (!ok) val = enumWorkingValues.value(row, row);
        enumWorkingValues[row] = val;
        refreshEnumDefaultCombo();
    }
}

void MainWidget::onEnumAddRowClicked()
{
    if (!enumTable || !enumTable->isEnabled()) return;
    QString newName = QString("Item%1").arg(enumWorkingItems.size() + 1);
    int newValue = enumWorkingValues.isEmpty() ? 0 : enumWorkingValues.last() + 1;
    enumWorkingItems.append(newName);
    enumWorkingValues.append(newValue);
    if (enumWorkingDefault.isEmpty()) enumWorkingDefault = newName;
    syncEnumEditor();
    enumTable->setCurrentCell(enumWorkingItems.size() - 1, 0);
}

void MainWidget::onEnumRemoveRowClicked()
{
    if (!enumTable || !enumTable->isEnabled()) return;
    int row = enumTable->currentRow();
    if (row < 0 || row >= enumWorkingItems.size()) return;
    QString removed = enumWorkingItems.at(row);
    enumWorkingItems.removeAt(row);
    if (row < enumWorkingValues.size()) enumWorkingValues.removeAt(row);
    if (enumWorkingDefault == removed) enumWorkingDefault.clear();
    syncEnumEditor();
}

void MainWidget::onEnumDefaultChanged(int index)
{
    if (!enumDefaultCombo || index < 0) return;
    enumWorkingDefault = enumDefaultCombo->itemData(index).toString();
}

void MainWidget::fillFormFromParam(const ParamMetadata* p)
{
    if (!p) return;
    formNameEdit->setText(p->name);
    const QString typeStr = paramTypeToString(p->type);
    {
        QSignalBlocker blocker(formTypeCombo);
        formTypeCombo->setCurrentText(typeStr);
    }
    formUnitEdit->setText(p->unit);
    formDefaultEdit->setText(p->defaultValue.isValid() ? p->defaultValue.toString() : QString());
    formDescEdit->setPlainText(p->description);

    {
        QSignalBlocker blocker(typeRefCombo);
        updateTypeRefCombo(typeStr);
    }

    arraySizeSpin->setVisible(typeStr == "char[]");
    if (typeStr == "char[]") arraySizeSpin->setValue(p->arraySize > 0 ? p->arraySize : 1);

    if (typeRefCombo->isVisible()) {
        int idx = -1;
        if (!p->typeName.isEmpty()) {
            idx = typeRefCombo->findData(p->typeName);
        } else if (typeStr == "enum") {
            idx = typeRefCombo->findData("LOCAL");
        }
        typeRefCombo->setCurrentIndex(idx < 0 ? 0 : idx);
    }

    if (typeStr == "enum" && enumEditorGroup) {
        enumEditorGroup->setVisible(true);

        const ParamMetadata* enumSource = p;
        if (!p->typeName.isEmpty()) {
            const ParamMetadata* typeDef = TypeManager::instance().getType(p->typeName);
            if (typeDef) enumSource = typeDef;
        }
        enumWorkingItems = enumSource ? enumSource->enumItems : QStringList();
        enumWorkingValues = enumSource ? enumSource->enumValues : QVector<int>();
        enumWorkingDefault = p->defaultValue.toString();
        syncEnumEditor();

        onTypeRefChanged(typeRefCombo->currentIndex());
    } else {
        if (enumEditorGroup) enumEditorGroup->setVisible(false);
        enumWorkingItems.clear();
        enumWorkingValues.clear();
        enumWorkingDefault.clear();
        if (enumDefaultCombo) enumDefaultCombo->setVisible(false);
    }
}

bool MainWidget::applyFormToParam(ParamMetadata* p, QString& error)
{
    if (!p) return false;
    const QString name = formNameEdit->text().trimmed();
    const QString typeStr = formTypeCombo->currentText();
    const QString unit = formUnitEdit->text();
    const QString defStr = formDefaultEdit->text().trimmed();
    const QString desc = formDescEdit->toPlainText();

    static QRegExp re("^[A-Za-z_][A-Za-z0-9_]*$");
    if (!re.exactMatch(name)) {
        error = QString::fromUtf8(u8"\u540D\u79F0\u4E0D\u5408\u6CD5");
        return false;
    }

    p->name = name;
    ParamType newType = stringToParamType(typeStr);

    if (p->type != newType) {
        p->type = newType;
        p->children.clear();
        p->typeName.clear();
        p->enumItems.clear();
        p->enumValues.clear();
    }

    p->unit = unit;
    p->description = desc;
    if (defStr.isEmpty()) p->defaultValue.clear();
    else p->defaultValue = defStr;
    if (typeStr == "char[]") p->arraySize = arraySizeSpin->value();
    if (p->type == ParamType::ENUM) {
        ensureEnumDefaultValid();
        if (!enumWorkingDefault.isEmpty()) p->defaultValue = enumWorkingDefault;
        if (p->typeName.isEmpty()) {
            normalizeEnumWorkingValues();
            p->enumItems = enumWorkingItems;
            p->enumValues = enumWorkingValues;
        }
    }

    if (p->type == ParamType::STRUCT || p->type == ParamType::ENUM) {
        if (typeRefCombo && typeRefCombo->isVisible()) {
            QString selData = typeRefCombo->currentData().toString();

            if (selData == "NEW") {
                bool ok = false;
                QString newTypeName = QInputDialog::getText(this, QString::fromUtf8(u8"新建类型"),
                                                            QString::fromUtf8(u8"请输入新类型名称(全局唯一):"), QLineEdit::Normal, "", &ok);
                if (ok && !newTypeName.isEmpty()) {
                    if (TypeManager::instance().hasType(newTypeName)) {
                        error = QString::fromUtf8(u8"类型名称已存在");
                        return false;
                    }
                    ParamMetadata meta;
                    meta.name = newTypeName;
                    meta.type = p->type;
                    if (p->type == ParamType::ENUM) {
                        meta.enumItems = p->enumItems;
                        meta.enumValues = p->enumValues;
                    }
                    TypeManager::instance().registerType(newTypeName, meta);
                    selData = newTypeName;
                } else {
                    error = QString::fromUtf8(u8"必须输入类型名称");
                    return false;
                }
            }

            if (selData == "LOCAL") {
                p->typeName.clear();
            } else {
                p->typeName = selData;
                p->children.clear();
                if (p->type == ParamType::ENUM) {
                    p->enumItems.clear();
                    p->enumValues.clear();
                }
            }
        }
    } else {
        p->typeName.clear();
    }

    return true;
}

void MainWidget::onTypeChanged(const QString& typeName)
{
    const bool isEnum = (typeName == "enum");
    const bool isStruct = (typeName == "struct");
    const bool isCharArr = (typeName == "char[]");

    if (arraySizeSpin) arraySizeSpin->setVisible(isCharArr);
    updateTypeRefCombo(typeName);

    if (enumEditorGroup) enumEditorGroup->setVisible(isEnum);

    if (!isEnum) {
        enumWorkingItems.clear();
        enumWorkingValues.clear();
        enumWorkingDefault.clear();
        refreshEnumDefaultCombo();
        if (enumTable) enumTable->setRowCount(0);
    } else {
        if (enumWorkingItems.isEmpty()) {
            enumWorkingItems << QStringLiteral("Item1");
            enumWorkingValues << 0;
            enumWorkingDefault = enumWorkingItems.first();
        }
        syncEnumEditor();
        if (typeRefCombo) {
            int localIdx = typeRefCombo->findData("LOCAL");
            if (localIdx >= 0) typeRefCombo->setCurrentIndex(localIdx);
            onTypeRefChanged(typeRefCombo->currentIndex());
        }
    }
    Q_UNUSED(isStruct);
}

void MainWidget::updateTypeRefCombo(const QString& typeStr)
{
    if (!typeRefCombo) return;
    typeRefCombo->clear();
    if (typeStr != "struct" && typeStr != "enum") {
        typeRefCombo->setVisible(false);
        return;
    }
    typeRefCombo->setVisible(true);
    typeRefCombo->addItem(QString::fromUtf8(u8"<新建类型...>"), QString("NEW"));
    if (typeStr == "enum") {
        typeRefCombo->addItem(QString::fromUtf8(u8"<本地定义>"), QString("LOCAL"));
    }
    ParamType targetType = (typeStr == "struct" ? ParamType::STRUCT : ParamType::ENUM);
    QStringList types = TypeManager::instance().getTypeNames(targetType);
    for (const QString& tn : types) {
        typeRefCombo->addItem(tn, tn);
    }
}

void MainWidget::onTypeRefChanged(int index)
{
    if (index < 0 || !typeRefCombo) return;
    QString data = typeRefCombo->itemData(index).toString();
    const bool isLocal = (data == "LOCAL");
    const bool isEnum = (formTypeCombo && formTypeCombo->currentText() == "enum");

    if (isEnum) {
        bool enableEditor = isLocal;
        if (enumEditorGroup) enumEditorGroup->setVisible(enableEditor || !isLocal);

        if (enumTable) {
            enumTable->setEnabled(enableEditor);
            enumTable->setEditTriggers(enableEditor ? QAbstractItemView::AllEditTriggers : QAbstractItemView::NoEditTriggers);
        }
        if (enumAddRowBtn) enumAddRowBtn->setEnabled(enableEditor);
        if (enumRemoveRowBtn) enumRemoveRowBtn->setEnabled(enableEditor && enumTable && enumTable->currentRow() >= 0);

        if (!isLocal && data != "NEW" && data != "LOCAL") {
            const ParamMetadata* typeDef = TypeManager::instance().getType(data);
            if (typeDef) {
                enumWorkingItems = typeDef->enumItems;
                enumWorkingValues = typeDef->enumValues;
                syncEnumEditor();
            }
        }
    }
}

void MainWidget::onApplyFormClicked()
{
    ParamMetadata* p = nullptr;
    if (!getParamByPath(currentPath, p)) return;
    ParamMetadata backupRoot = rootParam;
    QString err;
    if (!applyFormToParam(p, err)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"\u65E0\u6CD5\u5E94\u7528"), err);
        return;
    }
    auto report = validateProject(rootParam);
    if (report.hasError()) {
        rootParam = backupRoot;
        showValidationReportDialog();
        QMessageBox::warning(this, QString::fromUtf8(u8"\u5E94\u7528\u5931\u8D25"), QString::fromUtf8(u8"\u8F93\u5165\u65E0\u6548\uFF0C\u5DF2\u56DE\u6EDA"));
        rebuildTreeFromModel();
        updateValidationStatus();
        return;
    }
    rebuildTreeFromModel();
    refreshCenterCanvas();
    updateValidationStatus();
}

void MainWidget::onCancelFormClicked()
{
    ParamMetadata* p = nullptr;
    if (!getParamByPath(currentPath, p)) return;
    fillFormFromParam(p);
}
