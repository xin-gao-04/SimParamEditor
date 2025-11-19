#ifndef MAIN_WIDGET_H
#define MAIN_WIDGET_H

#include <QList>
#include <QWidget>
#include <QStringList>
#include "param_types.h"

class QPushButton;
class QFileDialog;
class QMessageBox;
class QLabel;
class QFormLayout;
class QLineEdit;
class QComboBox;
class QTextEdit;
class QString;
class QStringList;
class QLayout;
class QHBoxLayout;
class QVBoxLayout;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;
class QScrollArea;
class QLineEdit;
class QComboBox;
class QTextEdit;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QTabWidget;
class QStackedWidget;
class QGroupBox;
class QTableWidget;
class QGroupBox;
class QListWidget;

// 主窗口类，负责UI的整体组织与核心逻辑，包括参数模板与实例的管理与编辑
class MainWidget : public QWidget
{
    Q_OBJECT
public:
    // 构造函数
    explicit MainWidget(QWidget* parent = nullptr);

private:
    // 构建主UI
    void buildUi();
    // 创建顶部工具栏
    QWidget* createTopBar();
    // 创建左侧参数结构树
    QWidget* createLeftOutline();
    // 创建中间参数展示区
    QWidget* createCenterCanvas();
    // 创建右侧属性编辑面板
    QWidget* createRightPropertyPanel();
    // 创建底部状态栏
    QWidget* createStatusBar();

    // 刷新参数（模板）视图
    void refreshCenterCanvas();
    // 刷新实例编辑区
    void refreshInstanceCanvas();

    // 根据路径获取参数类型节点
    const ParamMetadata* findTypeNodeByPath(const QString& path) const;

    // 编辑实例值（弹窗编辑）
    bool editInstanceValues(InstanceMetadata& inst);
    // 渲染实例编辑器（表单）
    QWidget* renderInstanceEditor(const InstanceMetadata& inst, const ParamMetadata& typeNode);

    // 通过实例名称查找实例对象
    bool findInstanceByName(const QString& name, InstanceMetadata*& outInst);
    // 设定实例的某路径下的值
    bool setInstanceValueByPath(InstanceMetadata& inst, const QStringList& relPath, const QString& value);

    // 根据模型数据重建参数树
    void rebuildTreeFromModel();
    // 递归创建树节点
    void createTreeItemsRecursively(QTreeWidgetItem* parentItem, const ParamMetadata& node, const QString& path);

    // 通过路径获取参数对象
    bool getParamByPath(const QString& path, ParamMetadata*& outParam);
    // 通过路径获取父参数对象及子索引
    bool getParentByPath(const QString& path, ParamMetadata*& outParent, int& childIndex);

    // 将参数信息填写到表单控件
    void fillFormFromParam(const ParamMetadata* p);
    // 应用表单内容事件
    bool applyFormToParam(ParamMetadata* p, QString& error);
    // 更新参数校验状态
    void updateValidationStatus();
    // 显示校验结果对话框
    void showValidationReportDialog();
    // 恢复上次树的选择
    void restoreTreeSelection();

    // 构建示例根节点
    void buildSampleRoot();
    // 重建实例树
    void rebuildInstancesTree();

private:
    // 主布局拆分器
    QSplitter* rootSplitter;
    // 左侧Tab控件（参数/实例切换）
    QTabWidget* leftTabs;
    // 模板页树（参数类型定义结构树，兼容原命名 outlineTree）
    QTreeWidget* outlineTree;
    // 实例页树（实例结构树）
    QTreeWidget* instancesTree;
    // 中间主展示区
    QStackedWidget* centerStack;
    QTreeWidget* propertyBrowser;
    QScrollArea* instanceScrollArea;
    QWidget* instanceContainer;
    QVBoxLayout* instanceLayout;
    // 右侧属性面板
    QWidget* propertyPanel;
    // 状态栏部件
    QWidget* statusBarWidget;
    // 状态显示标签
    QLabel* statusLabel;
    QPushButton* themeToggleBtn;
    QList<int> lastTemplateSplitterSizes;

    // 参数模板根节点数据
    ParamMetadata rootParam;
    // 上次打开项目路径
    QString lastProjectPath;
    // 上次导出代码的目录
    QString lastOutputDir;
    // 当前选择的参数路径（如用于操作定位、高亮）
    QString currentPath;
    // 当前是否在实例tab页
    bool onInstancesTab = false;
    // 当前被选中的实例名称
    QString currentInstanceName;

    // 表单控件们（右侧编辑器用）
    QLineEdit* formNameEdit;          // 名称
    QComboBox* formTypeCombo;         // 类型选择
    QLineEdit* formUnitEdit;          // 单位
    QLineEdit* formDefaultEdit;       // 默认值
    QTextEdit* formDescEdit;          // 变量说明
    QPushButton* formApplyBtn;        // 应用按钮
    QPushButton* formCancelBtn;       // 取消按钮
    QSpinBox* arraySizeSpin;          // char[]长度
    QComboBox* enumDefaultCombo;      // 枚举默认值下拉
    QComboBox* typeRefCombo;          // 结构/枚举的复用引用下拉
    QGroupBox* enumEditorGroup;
    QTableWidget* enumTable;
    QPushButton* enumAddRowBtn;
    QPushButton* enumRemoveRowBtn;
    QStringList enumWorkingItems;
    QVector<int> enumWorkingValues;
    QString enumWorkingDefault;
    bool suppressEnumTableSignal = false;
    QVector<InstanceMetadata> instances; // 当前项目的所有实例

    void updateThemeToggleButton();
    void updatePropertyPanelVisibility();
    // 刷新模板字段工作台（Property Browser）：围绕当前选中节点展示叶子字段列表
    void populatePropertyBrowser();
    void selectPropertyItem(const QString& path);
    void syncEnumEditor();
    void updateEnumButtonsState();
    void refreshEnumDefaultCombo();
    void ensureEnumDefaultValid();
    void normalizeEnumWorkingValues();
    void updateTypeRefCombo(const QString& typeStr);

private slots:
    // 新建项目
    void onNewProjectClicked();
    // 打开项目
    void onOpenClicked();
    // 保存项目
    void onSaveClicked();
    // 代码生成
    void onGenerateClicked();
    // 生成实例相关代码
    void onGenerateInstancesClicked();
    // 树选择变化处理
    void onTreeSelectionChanged();
    // 应用表单内容事件
    void onApplyFormClicked();
    // 取消表单/恢复原内容事件
    void onCancelFormClicked();
    // 模板树右键菜单处理
    void onOutlineContextMenuRequested(const QPoint& pos);
    // 实例树右键菜单处理
    void onInstancesContextMenuRequested(const QPoint& pos);
    // 显示参数合法性校验
    void onShowValidationClicked();
    // 类型下拉框变化
    void onTypeChanged(const QString& typeName);
    // 树节点拖动（移动）处理
    void onOutlineRowsMoved(const QModelIndex& srcParent, int start, int end, const QModelIndex& dstParent, int dstRow);
    // 删除选中参数
    void onDeleteSelected();
    // 重命名参数
    void onRenameSelected();
    // 实例编辑表单内容变化
    void onInstanceEditorEdited();
    // 实例字段下拉变化
    void onInstanceFieldComboChanged(int);
    void onThemeToggleClicked();
    void onEnumTableCellChanged(int row, int column);
    void onEnumAddRowClicked();
    void onEnumRemoveRowClicked();
    void onEnumDefaultChanged(int);
    void onTypeRefChanged(int index);

private:
    bool suppressTreeSelection = false;
    bool suppressPropertySelection = false;
};

#endif // MAIN_WIDGET_H
