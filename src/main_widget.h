#ifndef MAIN_WIDGET_H
#define MAIN_WIDGET_H

#include <QWidget>
//#include order kept minimal
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

#include "param_types.h"
#include <QSet>

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

class MainWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MainWidget(QWidget* parent = nullptr);

private:
    void buildUi();
    QWidget* createTopBar();
    QWidget* createLeftOutline();
    QWidget* createCenterCanvas();
    QWidget* createRightPropertyPanel();
    QWidget* createStatusBar();
    void refreshCenterCanvas();
    void refreshInstanceCanvas();
    QWidget* createParamCard(const ParamMetadata& node);
    void buildCardsRecursively(QVBoxLayout* layout, const ParamMetadata& node, const QString& path, int depth);
    void updateCardHeaderIndicator(QWidget* card, const QString& name, bool expanded);
    void renameExpandedPaths(const QString& oldPath, const QString& newPath);
    void pruneExpandedPaths(const QString& prefixPath);
    const ParamMetadata* findTypeNodeByPath(const QString& path) const;
    bool editInstanceValues(InstanceMetadata& inst);
    QWidget* renderInstanceEditor(const InstanceMetadata& inst, const ParamMetadata& typeNode);
    bool findInstanceByName(const QString& name, InstanceMetadata*& outInst);
    bool setInstanceValueByPath(InstanceMetadata& inst, const QStringList& relPath, const QString& value);
    void rebuildTreeFromModel();
    void createTreeItemsRecursively(QTreeWidgetItem* parentItem, const ParamMetadata& node, const QString& path);
    bool getParamByPath(const QString& path, ParamMetadata*& outParam);
    bool getParentByPath(const QString& path, ParamMetadata*& outParent, int& childIndex);
    void fillFormFromParam(const ParamMetadata* p);
    bool applyFormToParam(ParamMetadata* p, QString& error);
    void updateValidationStatus();
    void showValidationReportDialog();
    void restoreTreeSelection();
    bool eventFilter(QObject* watched, QEvent* event) override;
    void buildSampleRoot();
    void rebuildInstancesTree();

private:
    QSplitter* rootSplitter;
    QTabWidget* leftTabs;
    QTreeWidget* outlineTree;          // 模板页树（兼容原命名）
    QTreeWidget* instancesTree;        // 实例页树
    QWidget* canvasContainer;
    QScrollArea* canvasScrollArea;
    QWidget* propertyPanel;
    QWidget* statusBarWidget;
    QLabel* statusLabel;

    // data
    ParamMetadata rootParam;
    QString lastProjectPath;
    QString lastOutputDir;
    QString currentPath;
    bool onInstancesTab = false;
    QString currentInstanceName;

    // form widgets
    QLineEdit* formNameEdit;
    QComboBox* formTypeCombo;
    QLineEdit* formUnitEdit;
    QLineEdit* formDefaultEdit;
    QTextEdit* formDescEdit;
    QPushButton* formApplyBtn;
    QPushButton* formCancelBtn;
    QPushButton* enumEditBtn;
    QSpinBox* arraySizeSpin;
    QComboBox* enumDefaultCombo;
    QSet<QString> expandedSet;
    QVector<InstanceMetadata> instances;

private slots:
    void onNewProjectClicked();
    void onOpenClicked();
    void onSaveClicked();
    void onGenerateClicked();
    void onGenerateInstancesClicked();
    void onTreeSelectionChanged();
    void onApplyFormClicked();
    void onCancelFormClicked();
    void onOutlineContextMenuRequested(const QPoint& pos);
    void onInstancesContextMenuRequested(const QPoint& pos);
    void onShowValidationClicked();
    void onTypeChanged(const QString& typeName);
    void onEditEnumClicked();
    void onOutlineRowsMoved(const QModelIndex& srcParent, int start, int end, const QModelIndex& dstParent, int dstRow);
    void onToggleClicked();
    void onDeleteSelected();
    void onRenameSelected();
    void onInstanceEditorEdited();
    void onInstanceEnumEdited(int);
    void onInstanceFieldComboChanged(int);
};

#endif // MAIN_WIDGET_H


