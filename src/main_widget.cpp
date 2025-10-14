#include "main_widget.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QFrame>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QSpinBox>
#include <QSizePolicy>
#include <QEvent>
#include <QDialog>
#include <QListWidget>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QDialogButtonBox>
#include <limits>

#include "json_io.h"
#include "generator/cpp_generator.h"
#include "validation.h"
// 简单的枚举编辑对话框
class EnumEditorDialog : public QDialog {
public:
    explicit EnumEditorDialog(const QStringList& items, const QVector<int>& values, QWidget* parent=nullptr)
        : QDialog(parent), initValues(values)
    {
        setWindowTitle(QString::fromUtf8(u8"编辑枚举项"));
        auto* v = new QVBoxLayout(this);
        list = new QListWidget(this);
        for (int i = 0; i < items.size(); ++i) {
            QListWidgetItem* it = new QListWidgetItem(items.at(i), list);
            it->setData(Qt::UserRole, i < values.size()? values[i] : QVariant());
        }
        v->addWidget(list);
        auto* row1 = new QHBoxLayout();
        input = new QLineEdit(this);
        valueSpin = new QSpinBox(this); valueSpin->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        QPushButton* addBtn = new QPushButton(QString::fromUtf8(u8"添加"), this);
        QPushButton* updBtn = new QPushButton(QString::fromUtf8(u8"更新"), this);
        QPushButton* delBtn = new QPushButton(QString::fromUtf8(u8"删除"), this);
        row1->addWidget(new QLabel(QString::fromUtf8(u8"名称:"), this));
        row1->addWidget(input);
        row1->addWidget(new QLabel(QString::fromUtf8(u8"值:"), this));
        row1->addWidget(valueSpin);
        row1->addWidget(addBtn);
        row1->addWidget(updBtn);
        row1->addWidget(delBtn);
        v->addLayout(row1);
        auto* row2 = new QHBoxLayout();
        QPushButton* autoIncBtn = new QPushButton(QString::fromUtf8(u8"自动递增"), this);
        row2->addWidget(autoIncBtn);
        v->addLayout(row2);
        auto* actionRow = new QHBoxLayout();
        actionRow->addStretch(1);
        QPushButton* okBtn = new QPushButton(QString::fromUtf8(u8"确定"), this);
        QPushButton* cancelBtn = new QPushButton(QString::fromUtf8(u8"取消"), this);
        actionRow->addWidget(okBtn);
        actionRow->addWidget(cancelBtn);
        v->addLayout(actionRow);
        connect(addBtn, &QPushButton::clicked, this, [this](){
            const QString name = input->text().trimmed(); if (name.isEmpty()) return;
            int val = valueSpin->value();
            auto* it = new QListWidgetItem(name, list); it->setData(Qt::UserRole, val);
            list->setCurrentItem(it);
        });
        connect(updBtn, &QPushButton::clicked, this, [this](){
            const QString name = input->text().trimmed(); if (name.isEmpty()) return;
            int val = valueSpin->value();
            QListWidgetItem* cur = list->currentItem();
            if (cur) { cur->setText(name); cur->setData(Qt::UserRole, val); }
        });
        connect(delBtn, &QPushButton::clicked, this, [this](){ auto* it = list->currentItem(); if (it) delete it; });
        connect(autoIncBtn, &QPushButton::clicked, this, [this](){ autoIncrementValues(); });
        connect(list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* it){ if (!it) return; input->setText(it->text()); valueSpin->setValue(it->data(Qt::UserRole).toInt()); });
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        setMinimumSize(360, 300);
    }
    QStringList resultItems() const {
        QStringList out;
        for (int i = 0; i < list->count(); ++i) out << list->item(i)->text().trimmed();
        out.removeAll("");
        out.removeDuplicates();
        return out;
    }
    QVector<int> resultValues() const {
        QVector<int> vals; vals.reserve(list->count());
        for (int i = 0; i < list->count(); ++i) vals << list->item(i)->data(Qt::UserRole).toInt();
        return vals;
    }
private:
    QListWidget* list;
    QLineEdit* input;
    QSpinBox* valueSpin;
    QVector<int> initValues;
    void autoIncrementValues() {
        if (list->count() == 0) return;
        // 从第0项开始，用已有的值作为种子；若后项小于等于前项，则设为前项+1
        int last = list->item(0)->data(Qt::UserRole).toInt();
        for (int i = 1; i < list->count(); ++i) {
            int cur = list->item(i)->data(Qt::UserRole).toInt();
            if (cur <= last) cur = last + 1;
            list->item(i)->setData(Qt::UserRole, cur);
            last = cur;
        }
    }
};

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
      canvasContainer(nullptr),
      canvasScrollArea(nullptr),
      propertyPanel(nullptr)
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
    bar->setStyleSheet("#topBar{background:#FFFFFF; border-bottom:1px solid #E0E0E0;}");
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
    h->addWidget(btnNew);
    h->addWidget(btnOpen);
    h->addWidget(btnSave);
    h->addSpacing(12);
    h->addWidget(btnGen);
    h->addWidget(btnGenInst);
    h->addStretch(1);

    connect(btnNew, &QPushButton::clicked, this, &MainWidget::onNewProjectClicked);
    connect(btnOpen, &QPushButton::clicked, this, &MainWidget::onOpenClicked);
    connect(btnSave, &QPushButton::clicked, this, &MainWidget::onSaveClicked);
    connect(btnGen, &QPushButton::clicked, this, &MainWidget::onGenerateClicked);
    connect(btnGenInst, &QPushButton::clicked, this, &MainWidget::onGenerateInstancesClicked);

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
    });
    connect(outlineTree, &QTreeWidget::itemSelectionChanged, this, &MainWidget::onTreeSelectionChanged);
    connect(outlineTree, &QWidget::customContextMenuRequested, this, &MainWidget::onOutlineContextMenuRequested);

    // 实例树选择（占位接线，与模板共用 onTreeSelectionChanged 的显示逻辑可后续扩展）
    connect(instancesTree, &QTreeWidget::itemSelectionChanged, this, &MainWidget::onTreeSelectionChanged);
    connect(instancesTree, &QWidget::customContextMenuRequested, this, &MainWidget::onInstancesContextMenuRequested);

    return left;
}

static QWidget* makeCard(const QString& title, const QList<QPair<QString, QString> >& rows)
{
    auto* card = new QFrame;
    card->setFrameShape(QFrame::StyledPanel);
    card->setFrameShadow(QFrame::Raised);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(8, 8, 8, 8);
    v->setSpacing(6);

    auto* header = new QLabel(QString(" %1").arg(title), card);
    header->setStyleSheet("font-weight:600; background:#F8F9FA; padding:8px; border:1px solid #E0E0E0;");
    v->addWidget(header);

    for (const auto& p : rows) {
        auto* row = new QWidget(card);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 6, 0, 6);

        auto* name = new QLabel(p.first, row);
        auto* value = new QLabel(p.second, row);
        value->setStyleSheet("background:#F5F5F5; padding:2px 6px; border-radius:3px; font-family:'Courier New'; color:#666;");

        h->addWidget(name, 0);
        h->addStretch(1);
        h->addWidget(value, 0);
        v->addWidget(row);
    }

    return card;
}

QWidget* MainWidget::createCenterCanvas()
{
    canvasScrollArea = new QScrollArea(this);
    canvasScrollArea->setWidgetResizable(true);

    canvasContainer = new QWidget(canvasScrollArea);
    auto* v = new QVBoxLayout(canvasContainer);
    v->setContentsMargins(16, 16, 16, 16);
    v->setSpacing(12);
    canvasScrollArea->setWidget(canvasContainer);
    refreshCenterCanvas();
    return canvasScrollArea;
}

QWidget* MainWidget::createRightPropertyPanel()
{
    propertyPanel = new QWidget(this);
    // 右侧允许自由拉伸（不再锁死宽度）
    auto* v = new QVBoxLayout(propertyPanel);
    v->setContentsMargins(12, 12, 12, 12);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);

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
    enumEditBtn = new QPushButton(QString::fromUtf8(u8"\u7F16\u8F91\u679A\u4E3E\u9879"), propertyPanel);
    enumEditBtn->setVisible(false);
    connect(enumEditBtn, &QPushButton::clicked, this, &MainWidget::onEditEnumClicked);
    enumDefaultCombo = new QComboBox(propertyPanel);
    enumDefaultCombo->setVisible(false);
    connect(enumDefaultCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onInstanceEnumEdited(int)));
    arraySizeSpin = new QSpinBox(propertyPanel);
    arraySizeSpin->setRange(1, 65535);
    arraySizeSpin->setVisible(false);

    form->addRow(QString::fromUtf8(u8"\u53D8\u91CF\u540D\u79F0"), formNameEdit);
    form->addRow(QString::fromUtf8(u8"\u6570\u636E\u7C7B\u578B"), formTypeCombo);
    form->addRow(QString::fromUtf8(u8"\u5355\u4F4D"), formUnitEdit);
    form->addRow(QString::fromUtf8(u8"\u9ED8\u8BA4\u503C"), formDefaultEdit);
    form->addRow(QString::fromUtf8(u8"\u53D8\u91CF\u8BF4\u660E"), formDescEdit);
    form->addRow(QString::fromUtf8(u8"\u679A\u4E3E"), enumEditBtn);
    form->addRow(QString::fromUtf8(u8"\u679A\u4E3E\u9ED8\u8BA4\u503C"), enumDefaultCombo);
    form->addRow(QString::fromUtf8(u8"char[] \u957F\u5EA6"), arraySizeSpin);

    v->addLayout(form);

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
    v->addStretch(1);

    return propertyPanel;
}

QWidget* MainWidget::createParamCard(const ParamMetadata& node)
{
    QList<QPair<QString, QString> > rows;
    for (const auto& c : node.children) {
        if (c.type == ParamType::STRUCT) {
            rows << qMakePair(c.name, QString()); // 占位，展开状态在递归渲染时体现
        } else if (c.type == ParamType::CHAR_ARRAY && c.arraySize > 0) {
            rows << qMakePair(c.name, QString("char[") + QString::number(c.arraySize) + "]");
        } else if (isNumericType(c.type)) {
            rows << qMakePair(c.name, c.defaultValue.isValid()? c.defaultValue.toString() : QString("0"));
        } else if (c.type == ParamType::ENUM) {
            rows << qMakePair(c.name, c.defaultValue.isValid()? c.defaultValue.toString() : QString("enum"));
        } else {
            rows << qMakePair(c.name, paramTypeToString(c.type));
        }
    }
    QWidget* card = makeCard(node.name, rows);
    card->setObjectName(QString("card_") + node.name);
    card->installEventFilter(this);
    return card;
}

void MainWidget::refreshCenterCanvas()
{
    if (!canvasContainer) return;
    // clear layout children
    if (auto* v = qobject_cast<QVBoxLayout*>(canvasContainer->layout())) {
        QLayoutItem* item;
        while ((item = v->takeAt(0)) != nullptr) {
            if (item->widget()) { item->widget()->deleteLater(); }
            delete item;
        }
        for (const auto& c : rootParam.children) {
            QWidget* top = createParamCard(c);
            const QString path = QString("/") + rootParam.name + "/" + c.name;
            top->setProperty("cardFullPath", path);
            v->addWidget(top);
            if (expandedSet.contains(path)) {
                // 展开子结构
                buildCardsRecursively(v, c, path, 1);
            }
        }
        v->addStretch(1);
    }
}

bool MainWidget::findInstanceByName(const QString& name, InstanceMetadata*& outInst)
{
    for (auto& in : instances) if (in.name == name) { outInst = &in; return true; }
    outInst = nullptr; return false;
}

static void renderFieldsRecursive(QVBoxLayout* layout, const ParamMetadata& typeNode, InstanceMetadata& inst, const QStringList& relPath, QObject* receiver)
{
    for (const auto& c : typeNode.children) {
        QStringList childPath = relPath; childPath << c.name;
        if (c.type == ParamType::STRUCT) {
            // group box style
            QWidget* group = new QWidget; auto* v = new QVBoxLayout(group); v->setContentsMargins(8,8,8,8);
            QLabel* title = new QLabel("[" + c.name + "]", group); v->addWidget(title);
            // find/create child instance node
            InstanceMetadata* childInst = nullptr;
            for (auto& ci : inst.children) if (ci.name == c.name) { childInst = &ci; break; }
            if (!childInst) { InstanceMetadata tmp; tmp.name = c.name; tmp.typePath = inst.typePath + "/" + c.name; inst.children.append(tmp); childInst = &inst.children.last(); }
            renderFieldsRecursive(v, c, *childInst, childPath, receiver);
            layout->addWidget(group);
        } else {
            QWidget* row = new QWidget; auto* h = new QHBoxLayout(row); h->setContentsMargins(0,4,0,4);
            QLabel* name = new QLabel(c.name, row); h->addWidget(name);
            const QString relKey = childPath.join("/");
            if (c.type == ParamType::ENUM) {
                QComboBox* combo = new QComboBox(row);
                combo->addItems(c.enumItems);
                combo->setCurrentText(inst.values.value(c.name).toString());
                combo->setProperty("instRelPath", relKey);
                QObject::connect(combo, SIGNAL(currentIndexChanged(int)), receiver, SLOT(onInstanceFieldComboChanged(int)));
                h->addWidget(combo);
            } else if (c.type == ParamType::FLOAT || c.type == ParamType::DOUBLE) {
                QLineEdit* editor = new QLineEdit(row);
                editor->setValidator(new QDoubleValidator(editor));
                editor->setProperty("instRelPath", relKey);
                editor->setText(inst.values.value(c.name).toString());
                QObject::connect(editor, SIGNAL(editingFinished()), receiver, SLOT(onInstanceEditorEdited()));
                h->addWidget(editor);
            } else if (isNumericType(c.type)) {
                QLineEdit* editor = new QLineEdit(row);
                editor->setValidator(new QIntValidator(editor));
                editor->setProperty("instRelPath", relKey);
                editor->setText(inst.values.value(c.name).toString());
                QObject::connect(editor, SIGNAL(editingFinished()), receiver, SLOT(onInstanceEditorEdited()));
                h->addWidget(editor);
            } else {
                QLineEdit* editor = new QLineEdit(row);
                editor->setProperty("instRelPath", relKey);
                editor->setText(inst.values.value(c.name).toString());
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
    if (!canvasContainer) return;
    if (leftTabs && leftTabs->currentIndex() != 1) return;
    // clear
    if (auto* v = qobject_cast<QVBoxLayout*>(canvasContainer->layout())) {
        QLayoutItem* item; while ((item = v->takeAt(0)) != nullptr) { if (item->widget()) item->widget()->deleteLater(); delete item; }
        // pick current instance
        QString instName;
        auto sel = instancesTree->selectedItems(); if (!sel.isEmpty()) instName = sel.first()->text(0); else if (!instances.isEmpty()) instName = instances.first().name;
        currentInstanceName = instName;
        InstanceMetadata* inst = nullptr;
        if (!instName.isEmpty() && findInstanceByName(instName, inst)) {
            const ParamMetadata* tn = findTypeNodeByPath(inst->typePath);
            if (tn) v->addWidget(renderInstanceEditor(*inst, *tn));
        }
        v->addStretch(1);
    }
}

void MainWidget::onInstanceEditorEdited()
{
    if (!sender()) return;
    QLineEdit* editor = qobject_cast<QLineEdit*>(sender()); if (!editor) return;
    const QString rel = editor->property("instRelPath").toString();
    InstanceMetadata* inst = nullptr; if (!findInstanceByName(currentInstanceName, inst)) return;
    // set value by leaf name (flat under current struct) — 简化实现：直接按最后一段作为 key
    QStringList relPath = rel.split('/', QString::SkipEmptyParts);
    if (relPath.isEmpty()) return;
    inst->values[relPath.last()] = editor->text();
}

void MainWidget::onInstanceFieldComboChanged(int)
{
    if (!sender()) return;
    QComboBox* combo = qobject_cast<QComboBox*>(sender()); if (!combo) return;
    const QString rel = combo->property("instRelPath").toString();
    InstanceMetadata* inst = nullptr; if (!findInstanceByName(currentInstanceName, inst)) return;
    QStringList relPath = rel.split('/', QString::SkipEmptyParts);
    if (relPath.isEmpty()) return;
    inst->values[relPath.last()] = combo->currentText();
}

void MainWidget::buildCardsRecursively(QVBoxLayout* layout, const ParamMetadata& node, const QString& path, int depth)
{
    for (const auto& c : node.children) {
        if (c.type != ParamType::STRUCT) continue;
        QWidget* card = createParamCard(c);
        const QString childPath = path + "/" + c.name;
        card->setProperty("cardFullPath", childPath);
        card->setStyleSheet(QString("margin-left:%1px;").arg(depth * 20));
        layout->addWidget(card);
        if (expandedSet.contains(childPath)) {
            buildCardsRecursively(layout, c, childPath, depth + 1);
        }
    }
}

void MainWidget::updateCardHeaderIndicator(QWidget* card, const QString& name, bool expanded)
{
    // 预留接口：如果后续将 header 拆为布局与图标，可在此更新箭头指示。当前卡片文本本身不包含箭头。
    Q_UNUSED(card); Q_UNUSED(name); Q_UNUSED(expanded);
}

void MainWidget::renameExpandedPaths(const QString& oldPath, const QString& newPath)
{
    QSet<QString> updated;
    for (const QString& p : expandedSet) {
        if (p.startsWith(oldPath)) {
            updated.insert(p);
        }
    }
    for (const QString& p : updated) {
        expandedSet.remove(p);
        QString suffix = p.mid(oldPath.size());
        expandedSet.insert(newPath + suffix);
    }
}

void MainWidget::pruneExpandedPaths(const QString& prefixPath)
{
    QSet<QString> toRemove;
    for (const QString& p : expandedSet) if (p.startsWith(prefixPath)) toRemove.insert(p);
    for (const QString& p : toRemove) expandedSet.remove(p);
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
    for (const auto& c : typeNode.children) {
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
    statusBarWidget->setStyleSheet("#statusBar{background:#FFFFFF; border-top:1px solid #E0E0E0;}");
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
    formTypeCombo->setCurrentText(paramTypeToString(p->type));
    formUnitEdit->setText(p->unit);
    formDefaultEdit->setText(p->defaultValue.isValid()? p->defaultValue.toString() : QString());
    formDescEdit->setPlainText(p->description);
    const QString t = paramTypeToString(p->type);
    enumEditBtn->setVisible(t == "enum");
    arraySizeSpin->setVisible(t == "char[]");
    if (t == "char[]") arraySizeSpin->setValue(p->arraySize>0? p->arraySize : 1);
    if (t == "enum") {
        enumDefaultCombo->clear();
        // 以 “名称(值)” 显示，并将名称作为 itemData 存储
        for (int i = 0; i < p->enumItems.size(); ++i) {
            const QString name = p->enumItems.at(i);
            const int val = (i < p->enumValues.size()) ? p->enumValues.at(i) : i;
            enumDefaultCombo->addItem(QString("%1 (%2)").arg(name).arg(val), name);
        }
        // 选中当前默认值（按名称匹配）
        int match = -1; for (int i = 0; i < enumDefaultCombo->count(); ++i) if (enumDefaultCombo->itemData(i).toString() == p->defaultValue.toString()) { match = i; break; }
        enumDefaultCombo->setCurrentIndex(match < 0 ? 0 : match);
        enumDefaultCombo->setVisible(true);
    } else {
        enumDefaultCombo->setVisible(false);
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
    {
        ParamType newType = stringToParamType(typeStr);
        p->type = newType;
        // 若新类型不允许包含子项，清空 children，避免校验报错
        if (!canHaveChildren(newType) && !p->children.isEmpty()) {
            p->children.clear();
        }
    }
    p->unit = unit;
    p->description = desc;
    if (defStr.isEmpty()) p->defaultValue.clear();
    else p->defaultValue = defStr;
    if (typeStr == "char[]") p->arraySize = arraySizeSpin->value();
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
    InstanceMetadata gpsInst; gpsInst.name = "GPSModule"; gpsInst.typePath = "/" + rootParam.name + "/SensorModule/GPSModule";
    gpsInst.values.insert("Latitude", QString("39.90f"));
    gpsInst.values.insert("Longitude", QString("116.40f"));
    gpsInst.values.insert("Altitude", 50);
    radar1.children.append(gpsInst);
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
            // 模板页：联动滚动
            QStringList parts = path.split('/', QString::SkipEmptyParts);
            if (parts.size() >= 2) {
                const QString top = parts.at(1);
                if (auto* v = qobject_cast<QVBoxLayout*>(canvasContainer->layout())) {
                    for (int i = 0; i < v->count(); ++i) {
                        QWidget* w = v->itemAt(i)->widget();
                        if (!w) continue;
                        if (w->objectName() == QString("card_") + top) {
                            canvasScrollArea->ensureWidgetVisible(w, 0, 20);
                            break;
                        }
                    }
                }
            }
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

bool MainWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget* w = qobject_cast<QWidget*>(watched);
        if (w && w->objectName().startsWith("card_")) {
            // 优先使用完整路径属性，否则退化为顶层路径
            QString path = w->property("cardFullPath").toString();
            if (path.isEmpty()) {
                const QString top = w->objectName().mid(QString("card_").size());
                path = QString("/") + rootParam.name + "/" + top;
            }
            currentPath = path;
            // select in tree
            QList<QTreeWidgetItem*> items = outlineTree->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
            for (auto* it : items) {
                if (it->data(0, Qt::UserRole).toString() == path) {
                    outlineTree->setCurrentItem(it);
                    break;
                }
            }
            ParamMetadata* p = nullptr; if (getParamByPath(path, p)) fillFormFromParam(p);
            // 切换展开状态：点击 struct 卡片自身时折叠/展开
            QStringList parts = path.split('/', QString::SkipEmptyParts);
            if (!parts.isEmpty()) {
                const QString structPath = path; // 完整路径
                if (expandedSet.contains(structPath)) expandedSet.remove(structPath);
                else expandedSet.insert(structPath);
                refreshCenterCanvas();
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MainWidget::onTypeChanged(const QString& typeName)
{
    enumEditBtn->setVisible(typeName == "enum");
    arraySizeSpin->setVisible(typeName == "char[]");
}

void MainWidget::onEditEnumClicked()
{
    ParamMetadata* p = nullptr; if (!getParamByPath(currentPath, p)) return;
    if (!p) return;
    // 允许在类型下拉已切到 enum 但尚未应用时直接编辑
    const bool comboIsEnum = (formTypeCombo && formTypeCombo->currentText() == "enum");
    if (p->type != ParamType::ENUM && !comboIsEnum) return;
    if (p->type != ParamType::ENUM && comboIsEnum) p->type = ParamType::ENUM;
    EnumEditorDialog dlg(p->enumItems, p->enumValues, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QStringList newItems = dlg.resultItems();
    p->enumItems = newItems;
    p->enumValues = dlg.resultValues();
    // 顺序约束：若未递增则强制递增
    for (int i = 1; i < p->enumValues.size(); ++i) if (p->enumValues[i] <= p->enumValues[i-1]) p->enumValues[i] = p->enumValues[i-1] + 1;
    if (!p->defaultValue.isValid() || !p->enumItems.contains(p->defaultValue.toString())) {
        p->defaultValue = p->enumItems.isEmpty()? QVariant() : QVariant(p->enumItems.first());
    }
    // 刷新默认值下拉（名称(值)）
    enumDefaultCombo->clear();
    for (int i = 0; i < p->enumItems.size(); ++i) {
        const QString name = p->enumItems.at(i);
        const int val = (i < p->enumValues.size()) ? p->enumValues.at(i) : i;
        enumDefaultCombo->addItem(QString("%1 (%2)").arg(name).arg(val), name);
    }
    int match = -1; for (int i = 0; i < enumDefaultCombo->count(); ++i) if (enumDefaultCombo->itemData(i).toString() == p->defaultValue.toString()) { match = i; break; }
    enumDefaultCombo->setCurrentIndex(match < 0 ? 0 : match);
    updateValidationStatus();
    // 刷新实例编辑器（enum 下拉可用）
    refreshInstanceCanvas();
}

void MainWidget::onInstanceEnumEdited(int)
{
    ParamMetadata* p = nullptr; if (!getParamByPath(currentPath, p)) return;
    if (!p || p->type != ParamType::ENUM) return;
    // 取出 itemData（名称）作为实际默认值
    p->defaultValue = enumDefaultCombo->itemData(enumDefaultCombo->currentIndex()).toString();
}

void MainWidget::onOutlineContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = outlineTree->itemAt(pos);
    if (!item) return;
    const QString path = item->data(0, Qt::UserRole).toString();
    QMenu menu(this);
    QAction* actAddStruct = menu.addAction(QString::fromUtf8(u8"\u6DFB\u52A0 struct"));
    QAction* actAddField  = menu.addAction(QString::fromUtf8(u8"\u6DFB\u52A0\u5B57\u6BB5"));
    QAction* actRename    = menu.addAction(QString::fromUtf8(u8"\u91CD\u547D\u540D"));
    QAction* actMoveUp    = menu.addAction(QString::fromUtf8(u8"\u4E0A\u79FB"));
    QAction* actMoveDown  = menu.addAction(QString::fromUtf8(u8"\u4E0B\u79FB"));
    QAction* actDelete    = menu.addAction(QString::fromUtf8(u8"\u5220\u9664"));
    QAction* chosen = menu.exec(outlineTree->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    ParamMetadata* p = nullptr;
    if (!getParamByPath(path, p)) return;

    if (chosen == actAddStruct) {
        if (!canHaveChildren(p->type)) {
            QMessageBox::warning(this, QString::fromUtf8(u8"\u64CD\u4F5C\u65E0\u6548"), QString::fromUtf8(u8"\u8BE5\u7C7B\u578B\u4E0D\u80FD\u6DFB\u52A0\u5B50\u9879"));
            return;
        }
        ParamMetadata child; child.name = "NewStruct"; child.type = ParamType::STRUCT;
        p->children.append(child);
        rebuildTreeFromModel(); updateValidationStatus(); refreshCenterCanvas();
    } else if (chosen == actAddField) {
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

void MainWidget::onToggleClicked()
{
    // 预留：当前使用事件过滤器处理卡片点击展开/折叠
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
    bool ok=false; QString newName = QInputDialog::getText(this, QString::fromUtf8(u8"\u91CD\u547D\u540D"), QString::fromUtf8(u8"\u8F93\u5165\u65B0\u540D\u79F0"), QLineEdit::Normal, p->name, &ok);
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


