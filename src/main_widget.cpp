#include "main_widget.h"
#include "type_manager.h"
#include "ui/theme_manager.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTabWidget>
#include <QScrollArea>
#include <QStackedWidget>
#include <QLabel>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QSpinBox>
#include <QSizePolicy>
#include <QDialog>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QSignalBlocker>
#include <QListWidget>
#include <limits>
#include <QApplication>

#include "json_io.h"
#include "generator/cpp_generator.h"
#include "validation.h"
// 实例选择对话框（默认全选）
class InstancesSelectDialog : public QDialog {
public:
    explicit InstancesSelectDialog(const QStringList& instanceNames, QWidget* parent=nullptr)
        : QDialog(parent)
    {
        setWindowTitle(QString::fromUtf8(u8"选择要生成的实例"));
        auto* v = new QVBoxLayout(this);
        list = new QListWidget(this);
        for (const auto& n : instanceNames) {
            auto* it = new QListWidgetItem(n, list);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            it->setCheckState(Qt::Checked);
        }
        v->addWidget(list);
        auto* h = new QHBoxLayout();
        QPushButton* allBtn = new QPushButton(QString::fromUtf8(u8"全选"), this);
        QPushButton* noneBtn = new QPushButton(QString::fromUtf8(u8"全不选"), this);
        h->addWidget(allBtn);
        h->addWidget(noneBtn);
        h->addStretch(1);
        v->addLayout(h);
        auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        v->addWidget(box);
        connect(allBtn, &QPushButton::clicked, this, [this](){ for (int i=0;i<list->count();++i) list->item(i)->setCheckState(Qt::Checked); });
        connect(noneBtn, &QPushButton::clicked, this, [this](){ for (int i=0;i<list->count();++i) list->item(i)->setCheckState(Qt::Unchecked); });
        setMinimumSize(360, 400);
    }
    QStringList selected() const {
        QStringList out; for (int i=0;i<list->count();++i) if (list->item(i)->checkState()==Qt::Checked) out<< list->item(i)->text(); return out;
    }
private:
    QListWidget* list;
};

MainWidget::MainWidget(QWidget* parent)
    : QWidget(parent),
      rootSplitter(nullptr),
      outlineTree(nullptr),
      centerStack(nullptr),
      propertyBrowser(nullptr),
      instanceScrollArea(nullptr),
      instanceContainer(nullptr),
      instanceLayout(nullptr),
      propertyPanel(nullptr),
      themeToggleBtn(nullptr)
{
    buildUi();
}

void MainWidget::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(createTopBar());
    rootSplitter = new QSplitter(Qt::Horizontal, this);
    rootSplitter->setChildrenCollapsible(false);
    rootSplitter->setStretchFactor(0, 0);
    rootSplitter->setStretchFactor(1, 1);
    rootSplitter->setStretchFactor(2, 0);

    rootSplitter->addWidget(createLeftOutline());
    rootSplitter->addWidget(createCenterCanvas());
    rootSplitter->addWidget(createRightPropertyPanel());

    QList<int> sizes;
    sizes << 520 << 700 << 280;
    rootSplitter->setSizes(sizes);
    lastTemplateSplitterSizes = sizes;

    layout->addWidget(rootSplitter);
    layout->addWidget(createStatusBar());
    layout->setStretch(0, 0);
    layout->setStretch(1, 1);
    layout->setStretch(2, 0);
    buildSampleRoot();
}

QWidget* MainWidget::createTopBar()
{
    auto* bar = new QWidget(this);
    bar->setObjectName("topBar");
    bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bar->setFixedHeight(40);
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(8, 6, 8, 6);
    h->setSpacing(8);
    auto* btnNew = new QPushButton(QString::fromUtf8(u8"\u65B0\u5EFA"), bar);
    auto* btnOpen = new QPushButton(QString::fromUtf8(u8"\u6253\u5F00"), bar);
    auto* btnSave = new QPushButton(QString::fromUtf8(u8"\u4FDD\u5B58"), bar);
    auto* btnGen = new QPushButton(QString::fromUtf8(u8"\u751F\u6210\u4EE3\u7801"), bar);
    auto* btnGenInst = new QPushButton(QString::fromUtf8(u8"\u751F\u6210\u5B9E\u4F8B"), bar);
    themeToggleBtn = new QPushButton(QString::fromUtf8(u8"\u2728 \u4E3B\u9898"), bar);

    btnNew->setProperty("class", "ghost");
    btnOpen->setProperty("class", "ghost");
    btnSave->setProperty("class", "ghost");
    btnGen->setProperty("class", "primary");
    btnGenInst->setProperty("class", "primary");
    themeToggleBtn->setProperty("class", "ghost");
    h->addWidget(btnNew);
    h->addWidget(btnOpen);
    h->addWidget(btnSave);
    h->addSpacing(12);
    h->addWidget(btnGen);
    h->addWidget(btnGenInst);
    h->addStretch(1);
    h->addWidget(themeToggleBtn);

    connect(btnNew, &QPushButton::clicked, this, &MainWidget::onNewProjectClicked);
    connect(btnOpen, &QPushButton::clicked, this, &MainWidget::onOpenClicked);
    connect(btnSave, &QPushButton::clicked, this, &MainWidget::onSaveClicked);
    connect(btnGen, &QPushButton::clicked, this, &MainWidget::onGenerateClicked);
    connect(btnGenInst, &QPushButton::clicked, this, &MainWidget::onGenerateInstancesClicked);
    connect(themeToggleBtn, &QPushButton::clicked, this, &MainWidget::onThemeToggleClicked);

    // 快捷键
    btnNew->setShortcut(QKeySequence("Ctrl+N"));
    btnOpen->setShortcut(QKeySequence("Ctrl+O"));
    btnSave->setShortcut(QKeySequence("Ctrl+S"));
    btnGen->setShortcut(QKeySequence("Ctrl+G"));
    btnGenInst->setShortcut(QKeySequence("Ctrl+Shift+G"));

    // 删除与重命名快捷键
    auto* delAct = new QAction(this); delAct->setShortcut(QKeySequence::Delete); addAction(delAct);
    connect(delAct, &QAction::triggered, this, &MainWidget::onDeleteSelected);
    auto* renAct = new QAction(this); renAct->setShortcut(Qt::Key_F2); addAction(renAct);
    connect(renAct, &QAction::triggered, this, &MainWidget::onRenameSelected);

    updateThemeToggleButton();
    return bar;
}

QWidget* MainWidget::createLeftOutline()
{
    auto* left = new QWidget(this);
    auto* v = new QVBoxLayout(left);
    v->setContentsMargins(0, 0, 0, 0);

    leftTabs = new QTabWidget(left);

    // 模板页
    QWidget* tplPage = new QWidget(leftTabs);
    auto* tplLayout = new QVBoxLayout(tplPage);
    tplLayout->setContentsMargins(0, 0, 0, 0);

    outlineTree = new QTreeWidget(tplPage);
    outlineTree->setHeaderHidden(true);
    outlineTree->setContextMenuPolicy(Qt::CustomContextMenu);
    tplLayout->addWidget(outlineTree);

    leftTabs->addTab(tplPage, QString::fromUtf8(u8"模板"));

    // 实例页
    QWidget* instPage = new QWidget(leftTabs);
    auto* instLayout = new QVBoxLayout(instPage);
    instLayout->setContentsMargins(0, 0, 0, 0);

    instancesTree = new QTreeWidget(instPage);
    instancesTree->setHeaderHidden(true);
    instancesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    instLayout->addWidget(instancesTree);

    leftTabs->addTab(instPage, QString::fromUtf8(u8"实例"));

    v->addWidget(leftTabs);

    // 初始化模板树示例
    auto* root = new QTreeWidgetItem(outlineTree, QStringList() << "PayloadConfig");
    auto* sensor = new QTreeWidgetItem(root, QStringList() << "SensorModule");
    new QTreeWidgetItem(sensor, QStringList() << "Temperature");
    new QTreeWidgetItem(sensor, QStringList() << "Humidity");
    new QTreeWidgetItem(sensor, QStringList() << "GPSModule");
    auto* comm = new QTreeWidgetItem(root, QStringList() << "CommunicationModule");
    new QTreeWidgetItem(comm, QStringList() << "Baudrate");
    new QTreeWidgetItem(comm, QStringList() << "Protocol");
    outlineTree->expandAll();

    // 连接信号：根据当前 tab 决定编辑上下文
    connect(leftTabs, &QTabWidget::currentChanged, this, [this](int idx){
        onInstancesTab = (idx == 1);
        if (onInstancesTab) {
            refreshInstanceCanvas();
        } else {
            refreshCenterCanvas();
            // 尝试用当前选择填充表单
            auto items = outlineTree->selectedItems();
            if (!items.isEmpty()) {
                ParamMetadata* p = nullptr;
                const QString path = items.first()->data(0, Qt::UserRole).toString();
                if (getParamByPath(path, p)) fillFormFromParam(p);
            }
        }
        updatePropertyPanelVisibility();
    });
    connect(outlineTree, &QTreeWidget::itemSelectionChanged, this, &MainWidget::onTreeSelectionChanged);
    connect(outlineTree, &QWidget::customContextMenuRequested, this, &MainWidget::onOutlineContextMenuRequested);

    // 实例树选择（占位接线，与模板共用 onTreeSelectionChanged 的显示逻辑可后续扩展）
    connect(instancesTree, &QTreeWidget::itemSelectionChanged, this, &MainWidget::onTreeSelectionChanged);
    connect(instancesTree, &QWidget::customContextMenuRequested, this, &MainWidget::onInstancesContextMenuRequested);

    return left;
}

QWidget* MainWidget::createCenterCanvas()
{
    centerStack = new QStackedWidget(this);

    propertyBrowser = new QTreeWidget(centerStack);
    propertyBrowser->setColumnCount(4);
    propertyBrowser->setHeaderLabels(QStringList()
                                     << QString::fromUtf8(u8"名称")
                                     << QString::fromUtf8(u8"类型")
                                     << QString::fromUtf8(u8"默认值")
                                     << QString::fromUtf8(u8"说明"));
    propertyBrowser->setSelectionMode(QAbstractItemView::SingleSelection);
    propertyBrowser->setSelectionBehavior(QAbstractItemView::SelectRows);
    propertyBrowser->setAlternatingRowColors(true);
    propertyBrowser->setIndentation(20);
    propertyBrowser->setRootIsDecorated(true);
    propertyBrowser->setUniformRowHeights(true);
    if (auto* header = propertyBrowser->header()) {
        header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        header->setStretchLastSection(true);
        header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(3, QHeaderView::Stretch);
    }
    centerStack->addWidget(propertyBrowser);

    connect(propertyBrowser, &QTreeWidget::itemSelectionChanged, this, [this]() {
        if (suppressPropertySelection) return;
        if (!outlineTree) return;
        const auto items = propertyBrowser->selectedItems();
        if (items.isEmpty()) return;
        const QString path = items.first()->data(0, Qt::UserRole).toString();
        if (path.isEmpty()) return;
        currentPath = path;
        ParamMetadata* p = nullptr;
        if (getParamByPath(path, p)) fillFormFromParam(p);
        suppressTreeSelection = true;
        QList<QTreeWidgetItem*> all = outlineTree->findItems(QStringLiteral("*"), Qt::MatchWildcard | Qt::MatchRecursive);
        for (auto* it : all) {
            if (it->data(0, Qt::UserRole).toString() == path) {
                outlineTree->setCurrentItem(it);
                break;
            }
        }
        suppressTreeSelection = false;
    });

    instanceScrollArea = new QScrollArea(centerStack);
    instanceScrollArea->setWidgetResizable(true);
    instanceContainer = new QWidget(instanceScrollArea);
    instanceLayout = new QVBoxLayout(instanceContainer);
    instanceLayout->setContentsMargins(16, 16, 16, 16);
    instanceLayout->setSpacing(12);
    instanceScrollArea->setWidget(instanceContainer);
    centerStack->addWidget(instanceScrollArea);

    return centerStack;
}

QWidget* MainWidget::createRightPropertyPanel()
{
    propertyPanel = new QWidget(this);
    propertyPanel->setObjectName("propertyPanel"); // QSS hook
    
    auto* v = new QVBoxLayout(propertyPanel);
    v->setContentsMargins(16, 16, 16, 16);
    v->setSpacing(16); // 增加控件间距

    // 标题
    auto* title = new QLabel(QString::fromUtf8(u8"属性编辑"), propertyPanel);
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #333; margin-bottom: 8px;");
    v->addWidget(title);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);
    form->setVerticalSpacing(12); // 表单行间距
    form->setHorizontalSpacing(12); // Label 和 Field 间距

    formNameEdit = new QLineEdit("Temperature", propertyPanel);
    formTypeCombo = new QComboBox(propertyPanel);
    formTypeCombo->addItems(QStringList() << "uint8" << "uint16" << "uint32"
                                      << "int8" << "int16" << "int32"
                                      << "float" << "double" << "char" << "char[]" << "enum" << "struct");
    formTypeCombo->setCurrentText("uint16");
    connect(formTypeCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(onTypeChanged(QString)));
    formUnitEdit = new QLineEdit(QString::fromUtf8(u8"\u00B0C"), propertyPanel);
    formDefaultEdit = new QLineEdit("25", propertyPanel);
    formDescEdit = new QTextEdit(propertyPanel);
    formDescEdit->setPlainText(QString::fromUtf8(u8"\u73AF\u5883\u6E29\u5EA6\u4F20\u611F\u5668\uFF0C\u6D4B\u91CF\u8303\u56F4 -40\u00B0C \u81F3 85\u00B0C\uFF0C\u7CBE\u5EA6 \u00B10.5\u00B0C"));
    enumDefaultCombo = new QComboBox(propertyPanel);
    enumDefaultCombo->setVisible(false);
    connect(enumDefaultCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onEnumDefaultChanged(int)));
    typeRefCombo = new QComboBox(propertyPanel);
    typeRefCombo->setVisible(false);
    arraySizeSpin = new QSpinBox(propertyPanel);
    arraySizeSpin->setRange(1, 65535);
    arraySizeSpin->setVisible(false);

    form->addRow(QString::fromUtf8(u8"\u53D8\u91CF\u540D\u79F0"), formNameEdit);
    form->addRow(QString::fromUtf8(u8"\u6570\u636E\u7C7B\u578B"), formTypeCombo);
    form->addRow(QString::fromUtf8(u8"\u5355\u4F4D"), formUnitEdit);
    form->addRow(QString::fromUtf8(u8"\u9ED8\u8BA4\u503C"), formDefaultEdit);
    form->addRow(QString::fromUtf8(u8"\u53D8\u91CF\u8BF4\u660E"), formDescEdit);
    form->addRow(QString::fromUtf8(u8"\u679A\u4E3E\u9ED8\u8BA4\u503C"), enumDefaultCombo);
    form->addRow(QString::fromUtf8(u8"\u590D\u7528\u5F15\u7528"), typeRefCombo);
    form->addRow(QString::fromUtf8(u8"char[] \u957F\u5EA6"), arraySizeSpin);

    v->addLayout(form);

    enumEditorGroup = new QGroupBox(QString::fromUtf8(u8"\u679A\u4E3E\u9879"), propertyPanel);
    auto* enumLayout = new QVBoxLayout(enumEditorGroup);
    enumTable = new QTableWidget(enumEditorGroup);
    enumTable->setColumnCount(2);
    enumTable->setHorizontalHeaderLabels(QStringList()
                                         << QString::fromUtf8(u8"\u540D\u79F0")
                                         << QString::fromUtf8(u8"\u6570\u503C"));
    enumTable->horizontalHeader()->setStretchLastSection(true);
    enumTable->verticalHeader()->setVisible(false);
    enumTable->setSelectionMode(QAbstractItemView::SingleSelection);
    enumTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    enumTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    enumLayout->addWidget(enumTable);
    auto* enumButtonsRow = new QHBoxLayout();
    enumAddRowBtn = new QPushButton(QString::fromUtf8(u8"\u6DFB\u52A0"), enumEditorGroup);
    enumRemoveRowBtn = new QPushButton(QString::fromUtf8(u8"\u5220\u9664"), enumEditorGroup);
    enumButtonsRow->addWidget(enumAddRowBtn);
    enumButtonsRow->addWidget(enumRemoveRowBtn);
    enumButtonsRow->addStretch(1);
    enumLayout->addLayout(enumButtonsRow);
    v->addWidget(enumEditorGroup);
    enumEditorGroup->setVisible(false);
    enumTable->setEnabled(false);
    enumAddRowBtn->setEnabled(false);
    enumRemoveRowBtn->setEnabled(false);

    connect(enumTable, &QTableWidget::cellChanged, this, &MainWidget::onEnumTableCellChanged);
    connect(enumTable, &QTableWidget::itemSelectionChanged, this, &MainWidget::updateEnumButtonsState);
    connect(enumAddRowBtn, &QPushButton::clicked, this, &MainWidget::onEnumAddRowClicked);
    connect(enumRemoveRowBtn, &QPushButton::clicked, this, &MainWidget::onEnumRemoveRowClicked);

    auto* btnRow = new QWidget(propertyPanel);
    auto* hb = new QHBoxLayout(btnRow);
    hb->setContentsMargins(0, 0, 0, 0);
    hb->setSpacing(8);
    formApplyBtn = new QPushButton(QString::fromUtf8(u8"\u5E94\u7528"), btnRow);
    formCancelBtn = new QPushButton(QString::fromUtf8(u8"\u53D6\u6D88"), btnRow);
    hb->addWidget(formApplyBtn);
    hb->addWidget(formCancelBtn);
    v->addWidget(btnRow);
    connect(formApplyBtn, &QPushButton::clicked, this, &MainWidget::onApplyFormClicked);
    connect(formCancelBtn, &QPushButton::clicked, this, &MainWidget::onCancelFormClicked);
    connect(typeRefCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onTypeRefChanged(int)));
    v->addStretch(1);

    return propertyPanel;
}

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
    propertyBrowser->clear();
    const QString rootPath = QString("/") + rootParam.name;
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(propertyBrowser);
    rootItem->setText(0, rootParam.name);
    rootItem->setText(1, QString::fromUtf8(u8"struct"));
    rootItem->setData(0, Qt::UserRole, rootPath);
    rootItem->setExpanded(true);
    populatePropertyItems(rootItem, rootParam, rootPath);
    propertyBrowser->expandToDepth(2);
}

void MainWidget::populatePropertyItems(QTreeWidgetItem* parentItem, const ParamMetadata& node, const QString& path)
{
    for (const auto& c : node.children) {
        const QString childPath = path + "/" + c.name;
        QTreeWidgetItem* item = new QTreeWidgetItem(parentItem);
        item->setText(0, c.name);
        item->setText(1, paramTypeToString(c.type));
        if (c.defaultValue.isValid()) {
            item->setText(2, c.defaultValue.toString());
        } else if (c.type == ParamType::CHAR_ARRAY && c.arraySize > 0) {
            item->setText(2, QString("char[%1]").arg(c.arraySize));
        } else {
            item->setText(2, "-");
        }
        item->setText(3, c.description);
        item->setData(0, Qt::UserRole, childPath);
        if (c.type == ParamType::STRUCT) {
            const ParamMetadata* childSrc = &c;
            if (!c.typeName.isEmpty()) {
                const ParamMetadata* def = TypeManager::instance().getType(c.typeName);
                if (def) childSrc = def;
            }
            populatePropertyItems(item, *childSrc, childPath);
            item->setExpanded(true);
        }
    }
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

bool MainWidget::findInstanceByName(const QString& name, InstanceMetadata*& outInst)
{
    for (auto& in : instances) if (in.name == name) { outInst = &in; return true; }
    outInst = nullptr; return false;
}

static void renderFieldsRecursive(QVBoxLayout* layout, const ParamMetadata& typeNode, InstanceMetadata& inst, const QStringList& relPath, QObject* receiver)
{
    // 如果 typeNode 本身是引用，需要先解析
    const ParamMetadata* src = &typeNode;
    if (!typeNode.typeName.isEmpty()) {
        const ParamMetadata* typeDef = TypeManager::instance().getType(typeNode.typeName);
        if (typeDef) src = typeDef;
    }

    for (const auto& c : src->children) {
        QStringList childPath = relPath; childPath << c.name;
        const QString flatKey = childPath.join("/");
        
        if (c.type == ParamType::STRUCT) {
            // group box style
            QWidget* group = new QWidget; auto* v = new QVBoxLayout(group); v->setContentsMargins(8,8,8,8);
            QLabel* title = new QLabel("[" + c.name + "]", group); v->addWidget(title);
            
            // 递归：不再查找子实例，直接传递当前实例和更深层的路径
            renderFieldsRecursive(v, c, inst, childPath, receiver);
            layout->addWidget(group);
        } else {
            QWidget* row = new QWidget; auto* h = new QHBoxLayout(row); h->setContentsMargins(0,4,0,4);
            QLabel* name = new QLabel(c.name, row);
            name->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            name->setMinimumWidth(180);
            h->addWidget(name);
            
            QVariant val = inst.values.value(flatKey);
            // 如果值不存在，尝试使用默认值
            if (!val.isValid()) val = c.defaultValue;

            if (c.type == ParamType::ENUM) {
                QComboBox* combo = new QComboBox(row);
                
                // 解析 Enum 定义
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
    // clone pointer to editable instance
    InstanceMetadata* inst = nullptr; if (!findInstanceByName(instConst.name, inst)) return new QWidget;
    QWidget* panel = new QWidget; auto* v = new QVBoxLayout(panel); v->setContentsMargins(12,12,12,12);
    QLabel* head = new QLabel(QString::fromUtf8(u8"编辑实例: ") + inst->name + "  (" + inst->typePath + ")", panel);
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
    QLineEdit* editor = qobject_cast<QLineEdit*>(sender()); if (!editor) return;
    const QString rel = editor->property("instRelPath").toString();
    InstanceMetadata* inst = nullptr; if (!findInstanceByName(currentInstanceName, inst)) return;
    // set value by flat key
    if (rel.isEmpty()) return;
    inst->values[rel] = editor->text();
}

void MainWidget::onInstanceFieldComboChanged(int)
{
    if (!sender()) return;
    QComboBox* combo = qobject_cast<QComboBox*>(sender()); if (!combo) return;
    const QString rel = combo->property("instRelPath").toString();
    InstanceMetadata* inst = nullptr; if (!findInstanceByName(currentInstanceName, inst)) return;
    if (rel.isEmpty()) return;
    inst->values[rel] = combo->currentText();
}

void MainWidget::updateThemeToggleButton()
{
    if (!themeToggleBtn) return;
    const bool isLight = ThemeManager::instance().currentTheme() == ThemeManager::ThemeVariant::Light;
    themeToggleBtn->setText(isLight ? QString::fromUtf8(u8"🌙 暗色") : QString::fromUtf8(u8"☀️ 亮色"));
    themeToggleBtn->setToolTip(isLight ? QString::fromUtf8(u8"切换到暗色主题") : QString::fromUtf8(u8"切换到亮色主题"));
}

void MainWidget::onThemeToggleClicked()
{
    ThemeManager::instance().toggleTheme(*qApp);
    updateThemeToggleButton();
}

void MainWidget::updatePropertyPanelVisibility()
{
    if (!rootSplitter || !propertyPanel) return;
    if (onInstancesTab) {
        if (propertyPanel->isVisible()) {
            auto sizes = rootSplitter->sizes();
            if (sizes.size() >= 3) {
                lastTemplateSplitterSizes = sizes;
                sizes[1] += sizes[2];
                sizes[2] = 0;
                rootSplitter->setSizes(sizes);
            }
            propertyPanel->setVisible(false);
        }
    } else {
        if (!propertyPanel->isVisible()) {
            propertyPanel->setVisible(true);
            if (lastTemplateSplitterSizes.size() >= 3) {
                rootSplitter->setSizes(lastTemplateSplitterSizes);
            } else {
                QList<int> defaults;
                defaults << 520 << 700 << 280;
                rootSplitter->setSizes(defaults);
                lastTemplateSplitterSizes = defaults;
            }
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
        bool ok=false;
        for (const auto& c : cur->children) { if (c.name == parts[i]) { cur = &c; ok = true; break; } }
        if (!ok) return nullptr;
    }
    return cur;
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
    QStringList leafs; collectLeafFields(*typeNode, leafs);
    // 简化编辑：逐个弹框输入
    for (const auto& f : leafs) {
        const QString cur = inst.values.value(f).toString();
        bool ok=false; QString v = QInputDialog::getText(this, QString::fromUtf8(u8"编辑实例值"), f + QString::fromUtf8(u8" 的值"), QLineEdit::Normal, cur, &ok);
        if (!ok) continue;
        inst.values[f] = v;
    }
    return true;
}

QWidget* MainWidget::createStatusBar()
{
    statusBarWidget = new QWidget(this);
    statusBarWidget->setObjectName("statusBar");
    statusBarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    statusBarWidget->setFixedHeight(30);
    auto* h = new QHBoxLayout(statusBarWidget);
    h->setContentsMargins(8, 2, 8, 2);
    h->setSpacing(8);
    statusLabel = new QLabel(QString::fromUtf8(u8"\u6821\u9A8C\u72B6\u6001: -"), statusBarWidget);
    auto* btn = new QPushButton(QString::fromUtf8(u8"\u67E5\u770B\u62A5\u544A"), statusBarWidget);
    connect(btn, &QPushButton::clicked, this, &MainWidget::onShowValidationClicked);
    h->addWidget(statusLabel);
    h->addStretch(1);
    h->addWidget(btn);
    return statusBarWidget;
}

void MainWidget::rebuildTreeFromModel()
{
    outlineTree->clear();
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(outlineTree, QStringList() << rootParam.name);
    rootItem->setData(0, Qt::UserRole, rootParam.name); // path
    createTreeItemsRecursively(rootItem, rootParam, QString("/") + rootParam.name);
    outlineTree->expandAll();
}

void MainWidget::rebuildInstancesTree()
{
    if (!instancesTree) return;
    instancesTree->clear();
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(instancesTree, QStringList() << QString::fromUtf8(u8"实例"));
    for (const auto& inst : instances) {
        QTreeWidgetItem* item = new QTreeWidgetItem(rootItem, QStringList() << inst.name);
        item->setData(0, Qt::UserRole, inst.typePath);
    }
    instancesTree->expandAll();
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
    formDefaultEdit->setText(p->defaultValue.isValid()? p->defaultValue.toString() : QString());
    formDescEdit->setPlainText(p->description);
    
    // 更新复用引用列表
    {
        QSignalBlocker blocker(typeRefCombo);
        updateTypeRefCombo(typeStr);
    }

    arraySizeSpin->setVisible(typeStr == "char[]");
    if (typeStr == "char[]") arraySizeSpin->setValue(p->arraySize>0? p->arraySize : 1);
    
    if (typeRefCombo->isVisible()) {
        // 尝试选中当前 typeName
        int idx = -1;
        if (!p->typeName.isEmpty()) {
            idx = typeRefCombo->findData(p->typeName);
        } else if (typeStr == "enum") {
            // 如果是 Enum 且 typeName 为空，选中 "LOCAL"
            idx = typeRefCombo->findData("LOCAL");
        }
        
        // 如果当前没有选中且是 struct/enum，默认选中第一个 (新建类型)
        typeRefCombo->setCurrentIndex(idx < 0 ? 0 : idx);
    }
    
    if (typeStr == "enum" && enumEditorGroup) {
        enumEditorGroup->setVisible(true); // 初始显示，由 onTypeRefChanged 控制启用状态
        
        const ParamMetadata* enumSource = p;
        if (!p->typeName.isEmpty()) {
            const ParamMetadata* typeDef = TypeManager::instance().getType(p->typeName);
            if (typeDef) enumSource = typeDef;
        }
        enumWorkingItems = enumSource ? enumSource->enumItems : QStringList();
        enumWorkingValues = enumSource ? enumSource->enumValues : QVector<int>();
        enumWorkingDefault = p->defaultValue.toString();
        syncEnumEditor();
        
        // 触发一次联动以设置 enabled 状态
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
    if (!re.exactMatch(name)) { error = QString::fromUtf8(u8"\u540D\u79F0\u4E0D\u5408\u6CD5"); return false; }

    p->name = name;
        ParamType newType = stringToParamType(typeStr);
    
    // 类型变更处理
    if (p->type != newType) {
        p->type = newType;
        p->children.clear(); // 类型变了，子项通常要清空
        p->typeName.clear(); // 类型变了，之前的引用也失效
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
        if (!enumWorkingDefault.isEmpty())
            p->defaultValue = enumWorkingDefault;
        if (p->typeName.isEmpty()) {
            normalizeEnumWorkingValues();
            p->enumItems = enumWorkingItems;
            p->enumValues = enumWorkingValues;
        }
    }

    // 处理 struct/enum 的类型引用
    if (p->type == ParamType::STRUCT || p->type == ParamType::ENUM) {
        if (typeRefCombo && typeRefCombo->isVisible()) {
            QString selData = typeRefCombo->currentData().toString();
            
            if (selData == "NEW") {
                // 用户选择了新建类型，弹出输入框
                bool ok = false;
                QString newTypeName = QInputDialog::getText(this, QString::fromUtf8(u8"新建类型"), 
                                                            QString::fromUtf8(u8"请输入新类型名称(全局唯一):"), QLineEdit::Normal, "", &ok);
                if (ok && !newTypeName.isEmpty()) {
                    if (TypeManager::instance().hasType(newTypeName)) {
                        error = QString::fromUtf8(u8"类型名称已存在");
                        return false;
                    }
                    // 创建新类型定义
                    ParamMetadata meta;
                    meta.name = newTypeName;
                    meta.type = p->type;
                    // 如果是 enum，保留当前的 enumItems (如果是从非引用状态转过来的)
                    if (p->type == ParamType::ENUM) {
                        meta.enumItems = p->enumItems;
                        meta.enumValues = p->enumValues;
                    }
                    
                    TypeManager::instance().registerType(newTypeName, meta);
                    selData = newTypeName; // 选中新创建的类型
                } else {
                    error = QString::fromUtf8(u8"必须输入类型名称");
                    return false;
                }
            }
            
            if (selData == "LOCAL") {
                 p->typeName.clear(); // 本地定义
            } else {
                 p->typeName = selData;
                 // 清空本地定义，强制使用引用
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


void MainWidget::buildSampleRoot()
{
    rootParam.name = "PayloadConfig";
    rootParam.type = ParamType::STRUCT;

    ParamMetadata sensor; sensor.name = "SensorModule"; sensor.type = ParamType::STRUCT;
    ParamMetadata temperature; temperature.name = "Temperature"; temperature.type = ParamType::UINT16; temperature.unit = QString::fromUtf8(u8"\u00B0C"); temperature.defaultValue = 25;
    ParamMetadata humidity; humidity.name = "Humidity"; humidity.type = ParamType::UINT8; humidity.unit = "%"; humidity.defaultValue = 60;
    ParamMetadata gps; gps.name = "GPSModule"; gps.type = ParamType::STRUCT;
    ParamMetadata lat; lat.name = "Latitude"; lat.type = ParamType::FLOAT;
    ParamMetadata lon; lon.name = "Longitude"; lon.type = ParamType::FLOAT;
    ParamMetadata alt; alt.name = "Altitude"; alt.type = ParamType::UINT16; alt.unit = "m";
    gps.children = { lat, lon, alt };
    sensor.children = { temperature, humidity, gps };

    ParamMetadata comm; comm.name = "CommunicationModule"; comm.type = ParamType::STRUCT;
    ParamMetadata baud; baud.name = "Baudrate"; baud.type = ParamType::UINT32;
    ParamMetadata proto; proto.name = "Protocol"; proto.type = ParamType::ENUM; proto.enumItems = (QStringList() << "UART" << "SPI" << "I2C"); proto.defaultValue = "UART";
    comm.children = { baud, proto };

    rootParam.children = { sensor, comm };
    // 示例实例：Radar1 基于 SensorModule
    instances.clear();
    InstanceMetadata radar1; radar1.name = "Radar1"; radar1.typePath = "/" + rootParam.name + "/SensorModule";
    radar1.values.insert("Temperature", 25);
    radar1.values.insert("Humidity", 60);
    // flat key for nested
    radar1.values.insert("GPSModule/Latitude", QString("39.90f"));
    radar1.values.insert("GPSModule/Longitude", QString("116.40f"));
    radar1.values.insert("GPSModule/Altitude", 50);
    instances.append(radar1);

    rebuildTreeFromModel();
    refreshCenterCanvas();
    rebuildInstancesTree();
    updateValidationStatus();
}

void MainWidget::onNewProjectClicked()
{
    buildSampleRoot();
    QMessageBox::information(this, QString::fromUtf8(u8"\u65B0\u5EFA"), QString::fromUtf8(u8"\u5DF2\u521B\u5EFA\u793A\u4F8B\u9879\u76EE\u7ED3\u6784"));
}

void MainWidget::onOpenClicked()
{
    const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8(u8"\u6253\u5F00\u9879\u76EE"), lastProjectPath, "SimParamEditor (*.spe)");
    if (path.isEmpty()) return;
    ParamMetadata temp;
    QVector<InstanceMetadata> insts;
    if (SpeIO::loadProjectAll(path, temp, insts)) {
        rootParam = temp; instances = insts;
        lastProjectPath = path;
        QMessageBox::information(this, QString::fromUtf8(u8"\u6253\u5F00\u9879\u76EE"), QString::fromUtf8(u8"\u52A0\u8F7D\u6210\u529F"));
        rebuildTreeFromModel(); rebuildInstancesTree(); refreshCenterCanvas(); updateValidationStatus();
    } else {
        QMessageBox::warning(this, QString::fromUtf8(u8"\u6253\u5F00\u9879\u76EE"), QString::fromUtf8(u8"\u52A0\u8F7D\u5931\u8D25"));
    }
}

void MainWidget::onSaveClicked()
{
    QString path = lastProjectPath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, QString::fromUtf8(u8"\u4FDD\u5B58\u9879\u76EE"), QDir::homePath() + "/project.spe", "SimParamEditor (*.spe)");
        if (path.isEmpty()) return;
    }
    // block on validation errors
    {
        auto report = validateProject(rootParam);
        if (report.hasError()) {
            showValidationReportDialog();
            QMessageBox::warning(this, QString::fromUtf8(u8"\u4FDD\u5B58\u963B\u6B62"), QString::fromUtf8(u8"\u5B58\u5728\u9519\u8BEF\uFF0C\u8BF7\u4FEE\u590D\u540E\u518D\u4FDD\u5B58"));
            return;
        }
    }
    if (SpeIO::saveProjectAll(path, rootParam, instances)) {
        lastProjectPath = path;
        QMessageBox::information(this, QString::fromUtf8(u8"\u4FDD\u5B58\u9879\u76EE"), QString::fromUtf8(u8"\u4FDD\u5B58\u6210\u529F"));
    } else {
        QMessageBox::warning(this, QString::fromUtf8(u8"\u4FDD\u5B58\u9879\u76EE"), QString::fromUtf8(u8"\u4FDD\u5B58\u5931\u8D25"));
    }
}

void MainWidget::onGenerateClicked()
{
    const QString out = QFileDialog::getExistingDirectory(this, QString::fromUtf8(u8"\u9009\u62E9\u8F93\u51FA\u76EE\u5F55"), lastOutputDir.isEmpty()? QDir::homePath() : lastOutputDir);
    if (out.isEmpty()) return;
    // block on validation errors
    {
        auto report = validateProject(rootParam);
        if (report.hasError()) {
            showValidationReportDialog();
            QMessageBox::warning(this, QString::fromUtf8(u8"\u751F\u6210\u963B\u6B62"), QString::fromUtf8(u8"\u5B58\u5728\u9519\u8BEF\uFF0C\u8BF7\u4FEE\u590D\u540E\u518D\u751F\u6210"));
            return;
        }
    }
    CppGenerator gen;
    if (gen.generate(rootParam, out)) {
        lastOutputDir = out;
        QMessageBox::information(this, QString::fromUtf8(u8"\u751F\u6210\u4EE3\u7801"), QString::fromUtf8(u8"\u751F\u6210\u6210\u529F"));
    } else {
        QMessageBox::warning(this, QString::fromUtf8(u8"\u751F\u6210\u4EE3\u7801"), QString::fromUtf8(u8"\u751F\u6210\u5931\u8D25"));
    }
}

void MainWidget::onGenerateInstancesClicked()
{
    // 选择实例
    QStringList names; for (const auto& in : instances) names << in.name;
    InstancesSelectDialog sel(names, this);
    if (sel.exec() != QDialog::Accepted) return;
    const QStringList chosen = sel.selected();
    if (chosen.isEmpty()) return;
    QVector<InstanceMetadata> picked;
    for (const auto& in : instances) if (chosen.contains(in.name)) picked.push_back(in);
    const QString out = QFileDialog::getExistingDirectory(this, QString::fromUtf8(u8"选择输出目录"), lastOutputDir.isEmpty()? QDir::homePath() : lastOutputDir);
    if (out.isEmpty()) return;
    CppGenerator gen;
    bool okCode = gen.generateInstances(rootParam, picked, out);
    bool okJson = gen.generateInstancesJson(rootParam, picked, out);
    if (okCode && okJson) {
        lastOutputDir = out;
        QMessageBox::information(this, QString::fromUtf8(u8"生成实例代码"), QString::fromUtf8(u8"生成成功（含 JSON 读写）"));
    } else {
        QMessageBox::warning(this, QString::fromUtf8(u8"生成实例代码"), QString::fromUtf8(u8"生成失败"));
    }
}

void MainWidget::onTreeSelectionChanged()
{
    if (suppressTreeSelection) return;
    // 根据当前 Tab 选择不同树源
    QTreeWidget* activeTree = onInstancesTab ? instancesTree : outlineTree;
    auto items = activeTree->selectedItems();
    if (items.isEmpty()) return;
    QTreeWidgetItem* item = items.first();
    const QString path = onInstancesTab ? item->data(0, Qt::UserRole).toString() : item->data(0, Qt::UserRole).toString();
    currentPath = path;
    ParamMetadata* p = nullptr;
    if (getParamByPath(path, p)) {
        fillFormFromParam(p);
        if (!onInstancesTab) {
            selectPropertyItem(path);
        } else {
            // 实例页：刷新实例编辑器
            refreshInstanceCanvas();
        }
    }
}

void MainWidget::onApplyFormClicked()
{
    ParamMetadata* p = nullptr;
    if (!getParamByPath(currentPath, p)) return;
    // backup
    ParamMetadata backupRoot = rootParam;
    QString err;
    if (!applyFormToParam(p, err)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"\u65E0\u6CD5\u5E94\u7528"), err);
        return;
    }
    // validate after apply
    auto report = validateProject(rootParam);
    if (report.hasError()) {
        rootParam = backupRoot; // revert
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

bool MainWidget::getParentByPath(const QString& path, ParamMetadata*& outParent, int& childIndex)
{
    outParent = nullptr; childIndex = -1;
    if (path.isEmpty()) return false;
    QStringList parts = path.split('/', QString::SkipEmptyParts);
    if (parts.size() < 2) return false;
    if (parts.first() != rootParam.name) return false;
    ParamMetadata* cur = &rootParam;
    for (int i = 1; i < parts.size()-1; ++i) {
        bool found = false;
        for (int j = 0; j < cur->children.size(); ++j) {
            if (cur->children[j].name == parts[i]) {
                cur = &cur->children[j];
                found = true; break;
            }
        }
        if (!found) return false;
    }
    outParent = cur;
    for (int j = 0; j < cur->children.size(); ++j) {
        if (cur->children[j].name == parts.last()) { childIndex = j; return true; }
    }
    return false;
}

void MainWidget::updateValidationStatus()
{
    auto report = validateProject(rootParam);
    int errors = 0, warns = 0;
    for (const auto& i : report.issues) {
        if (i.level == ValidationIssue::Error) ++errors; else ++warns;
    }
    statusLabel->setText(QString::fromUtf8(u8"\u6821\u9A8C\u72B6\u6001: ") + (errors==0? QString::fromUtf8(u8"\u901A\u8FC7") : QString::fromUtf8(u8"\u9519\u8BEF ")+QString::number(errors)) +
                         QString::fromUtf8(u8"  \u8B66\u544A: ") + QString::number(warns));
}

void MainWidget::showValidationReportDialog()
{
    auto report = validateProject(rootParam);
    QString text;
    for (const auto& i : report.issues) {
        text += (i.level==ValidationIssue::Error? QString::fromUtf8(u8"[\u9519\u8BEF] ") : QString::fromUtf8(u8"[\u8B66\u544A] ")) + i.path + " - " + i.message + "\n";
    }
    if (text.isEmpty()) text = QString::fromUtf8(u8"\u65E0\u95EE\u9898");
    QMessageBox::information(this, QString::fromUtf8(u8"\u6821\u9A8C\u62A5\u544A"), text);
}

void MainWidget::onShowValidationClicked()
{
    showValidationReportDialog();
}

void MainWidget::onTypeChanged(const QString& typeName)
{
    const bool isEnum = (typeName == "enum");
    const bool isStruct = (typeName == "struct");
    const bool isCharArr = (typeName == "char[]");
    
    // 严格互斥显示
    if (arraySizeSpin) arraySizeSpin->setVisible(isCharArr);
    
    // 更新并显示/隐藏 TypeRefCombo
    updateTypeRefCombo(typeName);
    
    // Enum 编辑器组可见性初始控制
    if (enumEditorGroup) enumEditorGroup->setVisible(isEnum);

    if (!isEnum) {
        enumWorkingItems.clear();
        enumWorkingValues.clear();
        enumWorkingDefault.clear();
        refreshEnumDefaultCombo();
        if (enumTable) enumTable->setRowCount(0);
    } else {
        if (enumWorkingItems.isEmpty()) {
            // 默认占位行
            enumWorkingItems << QStringLiteral("Item1");
            enumWorkingValues << 0;
            enumWorkingDefault = enumWorkingItems.first();
        }
        syncEnumEditor();
        
        // 触发一次 TypeRef 联动以决定是否启用本地编辑
        // 默认 updateTypeRefCombo 选中 0 (NEW) 或 user selection
        // 此时 index 可能是 0
        if (typeRefCombo) {
             // 默认选中 LOCAL 如果可用? 不，updateTypeRefCombo 默认不选或选 0
             // 如果我们希望切换到 Enum 时默认是 Local:
             int localIdx = typeRefCombo->findData("LOCAL");
             if (localIdx >= 0) typeRefCombo->setCurrentIndex(localIdx);
             onTypeRefChanged(typeRefCombo->currentIndex());
        }
    }
}

void MainWidget::onOutlineContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = outlineTree->itemAt(pos);
    if (!item) return;
    const QString path = item->data(0, Qt::UserRole).toString();
    QMenu menu(this);
    QAction* actAddStruct = nullptr; // 禁止新增本地 struct，统一使用复用
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
        ParamMetadata child; child.name = "newField"; child.type = ParamType::UINT16;
        p->children.append(child);
        rebuildTreeFromModel(); updateValidationStatus(); refreshCenterCanvas();
    } else if (chosen == actRename) {
        bool ok=false; QString newName = QInputDialog::getText(this, QString::fromUtf8(u8"\u91CD\u547D\u540D"), QString::fromUtf8(u8"\u8F93\u5165\u65B0\u540D\u79F0"), QLineEdit::Normal, p->name, &ok);
        if (ok && !newName.isEmpty()) {
            QStringList parts = path.split('/', QString::SkipEmptyParts);
            parts.last() = newName;
            const QString newPath = QString("/") + parts.join("/");
            p->name = newName;
            rebuildTreeFromModel(); refreshCenterCanvas(); updateValidationStatus();
            currentPath = newPath;
            // 恢复树选中
            QList<QTreeWidgetItem*> items = outlineTree->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
            for (auto* it : items) { if (it->data(0, Qt::UserRole).toString() == newPath) { outlineTree->setCurrentItem(it); break; } }
        }
    } else if (chosen == actMoveUp || chosen == actMoveDown) {
        ParamMetadata* parent = nullptr; int idx=-1;
        if (!getParentByPath(path, parent, idx) || !parent) return;
        int newIdx = idx + (chosen == actMoveUp ? -1 : 1);
        if (newIdx < 0 || newIdx >= parent->children.size()) return;
        ParamMetadata tmp = parent->children[idx];
        parent->children[idx] = parent->children[newIdx];
        parent->children[newIdx] = tmp;
        rebuildTreeFromModel(); updateValidationStatus(); refreshCenterCanvas();
    } else if (chosen == actDelete) {
        auto ret = QMessageBox::question(this, QString::fromUtf8(u8"\u786E\u8BA4\u5220\u9664"), QString::fromUtf8(u8"\u786E\u5B9A\u5220\u9664\u8BE5\u9879\u5417\uFF1F"));
        if (ret != QMessageBox::Yes) return;
        ParamMetadata* parent = nullptr; int idx=-1;
        if (getParentByPath(path, parent, idx) && parent && idx>=0) { parent->children.remove(idx); rebuildTreeFromModel(); updateValidationStatus(); refreshCenterCanvas(); }
    }
}

void MainWidget::onInstancesContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = instancesTree->itemAt(pos);
    QMenu menu(this);
    QAction* actAdd = menu.addAction(QString::fromUtf8(u8"新建实例"));
    QAction* actEdit = menu.addAction(QString::fromUtf8(u8"编辑值"));
    QAction* actRename = nullptr; QAction* actDelete = nullptr;
    if (item) {
        actRename = menu.addAction(QString::fromUtf8(u8"重命名"));
        actDelete = menu.addAction(QString::fromUtf8(u8"删除"));
    }
    QAction* chosen = menu.exec(instancesTree->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actAdd) {
        // 选择类型路径（简化：枚举根下的一级 struct）
        QStringList types; for (const auto& c : rootParam.children) if (c.type == ParamType::STRUCT) types << c.name;
        bool ok=false; QString typeName = QInputDialog::getItem(this, QString::fromUtf8(u8"选择类型"), QString::fromUtf8(u8"从模板选择结构体"), types, 0, false, &ok);
        if (!ok || typeName.isEmpty()) return;
        bool ok2=false; QString instName = QInputDialog::getText(this, QString::fromUtf8(u8"新建实例"), QString::fromUtf8(u8"实例名称"), QLineEdit::Normal, "NewInstance", &ok2);
        if (!ok2 || instName.isEmpty()) return;
        InstanceMetadata in; in.name = instName; in.typePath = "/" + rootParam.name + "/" + typeName;
        instances.append(in);
        rebuildInstancesTree();
    } else if (chosen == actEdit) {
        if (!item) return;
        for (auto& in : instances) if (in.name == item->text(0)) { editInstanceValues(in); break; }
        rebuildInstancesTree();
    } else if (actRename && chosen == actRename) {
        bool ok=false; QString newName = QInputDialog::getText(this, QString::fromUtf8(u8"重命名"), QString::fromUtf8(u8"新名称"), QLineEdit::Normal, item->text(0), &ok);
        if (!ok || newName.isEmpty()) return;
        for (auto& in : instances) if (in.name == item->text(0)) { in.name = newName; break; }
        rebuildInstancesTree();
    } else if (actDelete && chosen == actDelete) {
        auto ret = QMessageBox::question(this, QString(u8"确认删除"), QString::fromUtf8(u8"确定删除该实例吗？"));
        if (ret != QMessageBox::Yes) return;
        for (int i = 0; i < instances.size(); ++i) if (instances[i].name == item->text(0)) { instances.remove(i); break; }
        rebuildInstancesTree();
    }
}

void MainWidget::onOutlineRowsMoved(const QModelIndex& srcParent, int start, int end, const QModelIndex& dstParent, int dstRow)
{
    Q_UNUSED(end);
    Q_UNUSED(dstParent);
    // map to parent param
    QTreeWidgetItem* parentItem = nullptr;
    if (srcParent.isValid()) parentItem = outlineTree->topLevelItem(0); // fallback
    if (!parentItem) parentItem = outlineTree->invisibleRootItem()->child(0);
    QString parentPath = parentItem? parentItem->data(0, Qt::UserRole).toString() : QString();
    ParamMetadata* parentParam = nullptr; if (!getParamByPath(parentPath, parentParam)) return;
    if (!parentParam) return;
    if (start < 0 || start >= parentParam->children.size()) return;
    int insertPos = dstRow; if (insertPos > parentParam->children.size()) insertPos = parentParam->children.size();
    if (insertPos == start) return;
    auto moved = parentParam->children.takeAt(start);
    if (insertPos > start) --insertPos; // Qt semantics
    parentParam->children.insert(insertPos, moved);
    rebuildTreeFromModel();
    refreshCenterCanvas();
    updateValidationStatus();
}

void MainWidget::onDeleteSelected()
{
    auto items = outlineTree->selectedItems(); if (items.isEmpty()) return;
    const QString path = items.first()->data(0, Qt::UserRole).toString();
    auto ret = QMessageBox::question(this, QString::fromUtf8(u8"\u786E\u8BA4\u5220\u9664"), QString::fromUtf8(u8"\u786E\u5B9A\u5220\u9664\u8BE5\u9879\u5417\uFF1F"));
    if (ret != QMessageBox::Yes) return;
    ParamMetadata* parent = nullptr; int idx = -1;
    if (getParentByPath(path, parent, idx) && parent && idx>=0) {
        parent->children.remove(idx);
        rebuildTreeFromModel(); refreshCenterCanvas(); updateValidationStatus();
    }
}

void MainWidget::onRenameSelected()
{
    auto items = outlineTree->selectedItems(); if (items.isEmpty()) return;
    const QString path = items.first()->data(0, Qt::UserRole).toString();
    ParamMetadata* p = nullptr; if (!getParamByPath(path, p)) return;
    bool ok=false; QString newName = QInputDialog::getText(this, QString::fromUtf8(u8"重命名"), QString::fromUtf8(u8"输入新名称"), QLineEdit::Normal, p->name, &ok);
    if (ok && !newName.isEmpty()) {
        QStringList parts = path.split('/', QString::SkipEmptyParts);
        parts.last() = newName;
        const QString newPath = QString("/") + parts.join("/");
        p->name = newName;
        rebuildTreeFromModel(); refreshCenterCanvas(); updateValidationStatus();
        currentPath = newPath;
        QList<QTreeWidgetItem*> list = outlineTree->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
        for (auto* it : list) { if (it->data(0, Qt::UserRole).toString() == newPath) { outlineTree->setCurrentItem(it); break; } }
    }
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

    // 1. 选项: 新建类型
    typeRefCombo->addItem(QString::fromUtf8(u8"<新建类型...>"), QString("NEW"));

    // 2. 选项: 本地定义 (仅枚举可用，Struct 强制复用或新建)
    if (typeStr == "enum") {
        typeRefCombo->addItem(QString::fromUtf8(u8"<本地定义>"), QString("LOCAL"));
    }

    // 3. 列出 TypeManager 中的现有类型
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
    
    // 仅当是 Enum 且选择了本地定义时，启用枚举编辑器
    if (isEnum) {
        // 如果是 NEW，暂时认为是引用/新建流程，不启用本地编辑器
        // 如果是具体引用，也不启用
        bool enableEditor = isLocal;

        if (enumEditorGroup) enumEditorGroup->setVisible(enableEditor || !isLocal);
        
        if (enumTable) {
            enumTable->setEnabled(enableEditor);
            enumTable->setEditTriggers(enableEditor ? QAbstractItemView::AllEditTriggers : QAbstractItemView::NoEditTriggers);
        }
        if (enumAddRowBtn) enumAddRowBtn->setEnabled(enableEditor);
        if (enumRemoveRowBtn) enumRemoveRowBtn->setEnabled(enableEditor && enumTable && enumTable->currentRow() >= 0);
        
        // 如果选择了引用类型，尝试填充预览数据
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
