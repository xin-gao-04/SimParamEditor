#include "ui/main_window/main_widget.h"

#include "ui/theme_manager.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

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

    auto* btnNew = new QPushButton(QStringLiteral("新建"), bar);
    auto* btnOpen = new QPushButton(QStringLiteral("打开"), bar);
    auto* btnSave = new QPushButton(QStringLiteral("保存"), bar);
    auto* btnGen = new QPushButton(QStringLiteral("生成代码"), bar);
    auto* btnGenInst = new QPushButton(QStringLiteral("生成实例"), bar);
    themeToggleBtn = new QPushButton(QStringLiteral("✨ 主题"), bar);

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

    btnNew->setShortcut(QKeySequence("Ctrl+N"));
    btnOpen->setShortcut(QKeySequence("Ctrl+O"));
    btnSave->setShortcut(QKeySequence("Ctrl+S"));
    btnGen->setShortcut(QKeySequence("Ctrl+G"));
    btnGenInst->setShortcut(QKeySequence("Ctrl+Shift+G"));

    auto* delAct = new QAction(this);
    delAct->setShortcut(QKeySequence::Delete);
    addAction(delAct);
    connect(delAct, &QAction::triggered, this, &MainWidget::onDeleteSelected);

    auto* renAct = new QAction(this);
    renAct->setShortcut(Qt::Key_F2);
    addAction(renAct);
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

    QWidget* tplPage = new QWidget(leftTabs);
    auto* tplLayout = new QVBoxLayout(tplPage);
    tplLayout->setContentsMargins(0, 0, 0, 0);

    outlineTree = new QTreeWidget(tplPage);
    outlineTree->setHeaderHidden(true);
    outlineTree->setContextMenuPolicy(Qt::CustomContextMenu);
    tplLayout->addWidget(outlineTree);
    leftTabs->addTab(tplPage, QStringLiteral("模板"));

    QWidget* instPage = new QWidget(leftTabs);
    auto* instLayout = new QVBoxLayout(instPage);
    instLayout->setContentsMargins(0, 0, 0, 0);

    instancesTree = new QTreeWidget(instPage);
    instancesTree->setHeaderHidden(true);
    instancesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    instLayout->addWidget(instancesTree);
    leftTabs->addTab(instPage, QStringLiteral("实例"));

    v->addWidget(leftTabs);

    connect(leftTabs, &QTabWidget::currentChanged, this, [this](int idx) {
        onInstancesTab = (idx == 1);
        if (onInstancesTab) {
            refreshInstanceCanvas();
        } else {
            refreshCenterCanvas();
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

    connect(instancesTree, &QTreeWidget::itemSelectionChanged, this, &MainWidget::onTreeSelectionChanged);
    connect(instancesTree, &QWidget::customContextMenuRequested, this, &MainWidget::onInstancesContextMenuRequested);

    return left;
}

QWidget* MainWidget::createCenterCanvas()
{
    centerStack = new QStackedWidget(this);

    propertyBrowser = new QTreeWidget(centerStack);
    propertyBrowser->setColumnCount(7);
    propertyBrowser->setHeaderLabels(QStringList()
                                     << QStringLiteral("名称")
                                     << QStringLiteral("相对路径")
                                     << QStringLiteral("类型")
                                     << QStringLiteral("单位")
                                     << QStringLiteral("默认值")
                                     << QStringLiteral("引用类型")
                                     << QStringLiteral("校验状态"));
    propertyBrowser->setSelectionMode(QAbstractItemView::SingleSelection);
    propertyBrowser->setSelectionBehavior(QAbstractItemView::SelectRows);
    propertyBrowser->setAlternatingRowColors(true);
    propertyBrowser->setIndentation(0);
    propertyBrowser->setRootIsDecorated(false);
    propertyBrowser->setUniformRowHeights(true);
    if (auto* header = propertyBrowser->header()) {
        header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        header->setStretchLastSection(true);
        header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(1, QHeaderView::Stretch);
        header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(6, QHeaderView::Stretch);
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
    propertyPanel->setObjectName("propertyPanel");

    auto* v = new QVBoxLayout(propertyPanel);
    v->setContentsMargins(16, 16, 16, 16);
    v->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("属性编辑"), propertyPanel);
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #333; margin-bottom: 8px;");
    v->addWidget(title);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);
    form->setVerticalSpacing(12);
    form->setHorizontalSpacing(12);

    formNameEdit = new QLineEdit("Temperature", propertyPanel);
    formTypeCombo = new QComboBox(propertyPanel);
    formTypeCombo->addItems(QStringList() << "uint8" << "uint16" << "uint32"
                                      << "int8" << "int16" << "int32"
                                      << "float" << "double" << "char" << "char[]" << "enum" << "struct");
    formTypeCombo->setCurrentText("uint16");
    formTypeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    formTypeCombo->setMinimumContentsLength(10);
    formTypeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(formTypeCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(onTypeChanged(QString)));
    formUnitEdit = new QLineEdit(QStringLiteral("°C"), propertyPanel);
    formDefaultEdit = new QLineEdit("25", propertyPanel);
    formDescEdit = new QTextEdit(propertyPanel);
    formDescEdit->setPlainText(QStringLiteral("环境温度传感器，测量范围 -40°C 至 85°C，精度 ±0.5°C"));
    enumDefaultCombo = new QComboBox(propertyPanel);
    enumDefaultCombo->setVisible(false);
    connect(enumDefaultCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onEnumDefaultChanged(int)));
    typeRefCombo = new QComboBox(propertyPanel);
    typeRefCombo->setVisible(false);
    arraySizeSpin = new QSpinBox(propertyPanel);
    arraySizeSpin->setRange(1, 65535);
    arraySizeSpin->setVisible(false);

    form->addRow(QStringLiteral("变量名称"), formNameEdit);
    form->addRow(QStringLiteral("数据类型"), formTypeCombo);
    form->addRow(QStringLiteral("单位"), formUnitEdit);
    form->addRow(QStringLiteral("默认值"), formDefaultEdit);
    form->addRow(QStringLiteral("变量说明"), formDescEdit);
    form->addRow(QStringLiteral("枚举默认值"), enumDefaultCombo);
    form->addRow(QStringLiteral("复用引用"), typeRefCombo);
    form->addRow(QStringLiteral("char[] 长度"), arraySizeSpin);
    v->addLayout(form);

    enumEditorGroup = new QGroupBox(QStringLiteral("枚举项"), propertyPanel);
    auto* enumLayout = new QVBoxLayout(enumEditorGroup);
    enumTable = new QTableWidget(enumEditorGroup);
    enumTable->setColumnCount(2);
    enumTable->setHorizontalHeaderLabels(QStringList()
                                         << QStringLiteral("名称")
                                         << QStringLiteral("数值"));
    enumTable->horizontalHeader()->setStretchLastSection(true);
    enumTable->verticalHeader()->setVisible(false);
    enumTable->setSelectionMode(QAbstractItemView::SingleSelection);
    enumTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    enumTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    enumLayout->addWidget(enumTable);
    auto* enumButtonsRow = new QHBoxLayout();
    enumAddRowBtn = new QPushButton(QStringLiteral("添加"), enumEditorGroup);
    enumRemoveRowBtn = new QPushButton(QStringLiteral("删除"), enumEditorGroup);
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
    formApplyBtn = new QPushButton(QStringLiteral("应用"), btnRow);
    formCancelBtn = new QPushButton(QStringLiteral("取消"), btnRow);
    hb->addWidget(formApplyBtn);
    hb->addWidget(formCancelBtn);
    v->addWidget(btnRow);
    connect(formApplyBtn, &QPushButton::clicked, this, &MainWidget::onApplyFormClicked);
    connect(formCancelBtn, &QPushButton::clicked, this, &MainWidget::onCancelFormClicked);
    connect(typeRefCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onTypeRefChanged(int)));
    v->addStretch(1);

    return propertyPanel;
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
    statusLabel = new QLabel(QStringLiteral("校验状态: -"), statusBarWidget);
    auto* btn = new QPushButton(QStringLiteral("查看报告"), statusBarWidget);
    connect(btn, &QPushButton::clicked, this, &MainWidget::onShowValidationClicked);
    h->addWidget(statusLabel);
    h->addStretch(1);
    h->addWidget(btn);
    return statusBarWidget;
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

void MainWidget::updateThemeToggleButton()
{
    if (!themeToggleBtn) return;
    const bool isLight = ThemeManager::instance().currentTheme() == ThemeManager::ThemeVariant::Light;
    themeToggleBtn->setText(isLight ? QStringLiteral("🌙 暗色") : QStringLiteral("☀️ 亮色"));
    themeToggleBtn->setToolTip(isLight ? QStringLiteral("切换到暗色主题") : QStringLiteral("切换到亮色主题"));
}

void MainWidget::onThemeToggleClicked()
{
    ThemeManager::instance().toggleTheme(*qApp);
    updateThemeToggleButton();
}
