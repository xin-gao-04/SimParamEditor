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
    leftTabs->addTab(tplPage, QString::fromUtf8(u8"模板"));

    QWidget* instPage = new QWidget(leftTabs);
    auto* instLayout = new QVBoxLayout(instPage);
    instLayout->setContentsMargins(0, 0, 0, 0);

    instancesTree = new QTreeWidget(instPage);
    instancesTree->setHeaderHidden(true);
    instancesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    instLayout->addWidget(instancesTree);
    leftTabs->addTab(instPage, QString::fromUtf8(u8"实例"));

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
                                     << QString::fromUtf8(u8"名称")
                                     << QString::fromUtf8(u8"相对路径")
                                     << QString::fromUtf8(u8"类型")
                                     << QString::fromUtf8(u8"单位")
                                     << QString::fromUtf8(u8"默认值")
                                     << QString::fromUtf8(u8"引用类型")
                                     << QString::fromUtf8(u8"校验状态"));
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

    auto* title = new QLabel(QString::fromUtf8(u8"属性编辑"), propertyPanel);
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
    themeToggleBtn->setText(isLight ? QString::fromUtf8(u8"\U0001F319 \u6697\u8272") : QString::fromUtf8(u8"\u2600\uFE0F \u4EAE\u8272"));
    themeToggleBtn->setToolTip(isLight ? QString::fromUtf8(u8"\u5207\u6362\u5230\u6697\u8272\u4E3B\u9898") : QString::fromUtf8(u8"\u5207\u6362\u5230\u4EAE\u8272\u4E3B\u9898"));
}

void MainWidget::onThemeToggleClicked()
{
    ThemeManager::instance().toggleTheme(*qApp);
    updateThemeToggleButton();
}
