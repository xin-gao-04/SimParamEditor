# SimParamEditor 开发说明文档

## 项目概述

**产品名称**: SimParamEditor  
**产品定位**: 仿真系统参数配置工具  
**目标用户**: 仿真系统开发者、测试工程师  
**技术栈**: Qt (C++ / QML)  
**版本规划**: MVP → V1.0 → V2.0

---

## 一、核心功能需求

### 1.1 数据类型支持

- **基础类型**: uint8, uint16, uint32, int8, int16, int32, float, double
- **字符类型**: char, char[] (固定长度数组)
- **枚举类型**: enum (支持自定义枚举项)
- **复杂类型**: struct (支持多重嵌套)

### 1.2 参数结构定义

每个参数包含以下属性：

```json
{
  "name": "Temperature",      // 变量名称
  "type": "uint16",           // 数据类型
  "unit": "°C",               // 单位
  "default": 25,              // 默认值
  "description": "环境温度",   // 变量说明
  "children": []              // 子参数（嵌套结构）
}
```

### 1.3 输出能力

- **JSON配置文件**: 保存参数结构定义
- **代码生成**:
    - C++ 头文件 (.h) 和实现文件 (.cpp)
    - Python 数据类 (.py)
    - JSON 模板文件 (用于运行时加载)
- **批量生成**: 每个复杂结构生成独立文件

---

## 二、UI架构设计

### 2.1 整体布局 (方案B：侧边栏混合模式)

```
┌─────────────────────────────────────────────────────────────┐
│ [文件] [编辑] [生成▼] [视图] [帮助]              [主题切换] │
├──────────┬──────────────────────────────────────┬───────────┤
│          │                                      │           │
│ 结构大纲 │         主编辑区 (画布)              │ 属性面板  │
│ (树形)   │         (卡片式布局)                  │ (表单)    │
│          │                                      │           │
│ 150px    │         自适应                        │ 250px     │
│          │                                      │           │
├──────────┴──────────────────────────────────────┴───────────┤
│ 状态栏: 校验状态 | 参数统计 | 生成进度                       │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 三大核心区域

#### A. 左侧结构大纲 (150px 固定宽度)

**组件**: QTreeWidget / QTreeView  
**功能**:

- 显示完整参数层级结构
- 支持展开/折叠节点
- 右键菜单快捷操作
- 拖拽调整参数顺序
- 搜索/过滤参数

**交互**:

```
▼ PayloadConfig (根节点)
  ▼ SensorModule
    • Temperature (uint16)
    • Humidity (uint8)
    ▼ GPSModule
      • Latitude (float)
      • Longitude (float)
      • Altitude (uint16)
  ▼ CommunicationModule
    ...

[+添加根结构] [🔍搜索] [↻刷新]
```

**实现要点**:

- 使用自定义 QTreeWidgetItem 存储参数元数据
- 节点图标根据类型动态显示（📊数值/📝文本/🎯枚举/📦结构）
- 单击选中 → 右侧属性面板更新
- 双击 → 主编辑区定位到对应卡片

#### B. 中间主编辑区 (画布)

**组件**: QScrollArea + 自定义 QWidget / QGraphicsView  
**布局**: 垂直流式布局

**卡片设计** (简化版):

```
┌─────────────────────────────────────────┐
│ 📦 SensorModule               [⚙️] [🗑️] │ ← 头部：类型图标+名称+操作按钮
├─────────────────────────────────────────┤
│  📊 Temperature  [25°C]       [编辑]     │ ← 参数行：图标+名称+值+操作
│  📊 Humidity     [60%]        [编辑]     │
│  📍 GPSModule    [展开▼]                │ ← 嵌套结构折叠显示
│                                         │
│  [+ 添加子参数] [右键菜单: 复制/删除]   │ ← 底部操作区
└─────────────────────────────────────────┘
```

**交互规则**:

- 卡片高度自适应内容
- 嵌套结构用缩进表示（每层 +20px）
- 最多显示3层，第4层自动折叠为 `[+3项]` 标签
- 悬停显示完整信息（QToolTip）
- 拖拽卡片调整顺序（显示插入预览线）

#### C. 右侧属性面板 (250px 固定宽度)

**组件**: QFormLayout / QStackedWidget  
**功能**: 编辑选中参数的详细属性

**表单布局**:

```
┌───────────────────────────┐
│ 当前选中: Temperature     │
├───────────────────────────┤
│ 变量名称                  │
│ ┌───────────────────────┐ │
│ │ Temperature           │ │
│ └───────────────────────┘ │
│                           │
│ 数据类型                  │
│ ┌───────────────────────┐ │
│ │ uint16          [▼]   │ │ ← QComboBox
│ └───────────────────────┘ │
│                           │
│ 单位                      │
│ ┌───────────────────────┐ │
│ │ °C                    │ │
│ └───────────────────────┘ │
│                           │
│ 默认值                    │
│ ┌───────────────────────┐ │
│ │ 25                    │ │ ← QSpinBox/QLineEdit
│ └───────────────────────┘ │
│                           │
│ 说明                      │
│ ┌───────────────────────┐ │
│ │ 环境温度传感器        │ │ ← QTextEdit
│ │                       │ │
│ └───────────────────────┘ │
│                           │
│ [✓ 应用] [✗ 取消]         │
└───────────────────────────┘
```

**实时校验**:

- 变量名：正则匹配 `[a-zA-Z_][a-zA-Z0-9_]*`，重名时红框警告
- 类型：下拉选择，enum 时显示额外配置项
- 默认值：根据类型限制范围（uint16: 0-65535）
- 修改即应用（或提供应用/取消按钮）

---

## 三、功能模块设计

### 3.1 类型系统

#### 类型定义 (C++)

```cpp
enum class ParamType {
    UINT8, UINT16, UINT32,
    INT8, INT16, INT32,
    FLOAT, DOUBLE,
    CHAR, CHAR_ARRAY,
    ENUM, STRUCT
};

struct ParamMetadata {
    QString name;
    ParamType type;
    QString unit;
    QVariant defaultValue;
    QString description;
    QVector<ParamMetadata> children; // 嵌套子参数
    
    // 枚举专用
    QStringList enumItems;
    
    // 数组专用
    int arraySize;
};
```

#### 类型库管理

- 内置常用类型（不可修改）
- 用户自定义类型（保存到配置）
- 模板类型（官方 + 用户）

### 3.2 模板系统

#### 模板结构

```json
{
  "templateName": "标准传感器套装",
  "category": "官方模板",
  "description": "包含温湿度、GPS、加速度计",
  "parameters": [
    {
      "name": "SensorModule",
      "type": "struct",
      "children": [...]
    }
  ]
}
```

#### 模板管理器

- **官方模板**: 内置资源文件 (qrc:/templates/)
- **用户模板**: ~/.simparameditor/templates/
- **加载流程**: 选择模板 → 参数配置向导 → 插入到当前结构
- **保存流程**: 右键节点 → "保存为模板"

### 3.3 数据持久化

#### 项目文件格式 (.spe)

```json
{
  "version": "1.0",
  "projectName": "DronePayload",
  "created": "2025-10-11T10:30:00Z",
  "modified": "2025-10-11T15:45:00Z",
  "parameters": [
    {
      "name": "PayloadConfig",
      "type": "struct",
      "children": [...]
    }
  ],
  "settings": {
    "theme": "dark",
    "lastGeneratePath": "/path/to/output"
  }
}
```

#### 文件操作

- **新建**: 创建空白项目
- **打开**: QFileDialog 选择 .spe 文件
- **保存**: 覆盖当前文件
- **另存为**: 指定新文件路径
- **自动保存**: 每5分钟或修改100次触发

### 3.4 代码生成器

#### 生成流程

```
用户点击 [生成代码] 
  ↓
弹出生成配置对话框
  - 选择目标语言: [✓] C++  [✓] Python  [ ] JSON模板
  - 输出路径: /path/to/output
  - 命名规则: 驼峰/下划线
  - 附加选项: [ ] 生成序列化函数
  ↓
校验参数完整性
  - 检查必填项
  - 检查类型冲突
  - 检查命名规范
  ↓
生成文件
  - C++: Sensor.h, Sensor.cpp
  - Python: sensor.py
  - 进度显示: [████████░░] 80%
  ↓
完成通知
  - 底部状态栏: ✓ 已生成 3 个文件 [打开文件夹]
```

#### C++ 代码模板示例

```cpp
// Auto-generated by SimParamEditor
// DO NOT EDIT MANUALLY

#pragma once
#include <cstdint>

namespace Payload {

struct GPSModule {
    float latitude;     // 纬度 (deg)
    float longitude;    // 经度 (deg)
    uint16_t altitude;  // 高度 (m)
    
    // 序列化函数 (可选)
    void toJson(const QString& path);
    static GPSModule fromJson(const QString& path);
};

struct SensorModule {
    uint16_t temperature;  // 环境温度 (°C)
    uint8_t humidity;      // 湿度 (%)
    GPSModule gps;
};

} // namespace Payload
```

#### Python 代码模板示例

```python
# Auto-generated by SimParamEditor
# DO NOT EDIT MANUALLY

from dataclasses import dataclass
from typing import Optional

@dataclass
class GPSModule:
    latitude: float = 0.0    # 纬度 (deg)
    longitude: float = 0.0   # 经度 (deg)
    altitude: int = 0        # 高度 (m)

@dataclass
class SensorModule:
    temperature: int = 0     # 环境温度 (°C)
    humidity: int = 0        # 湿度 (%)
    gps: Optional[GPSModule] = None
```

---

## 四、交互设计细节

### 4.1 快捷键绑定

|快捷键|功能|
|---|---|
|Ctrl+N|新建项目|
|Ctrl+O|打开项目|
|Ctrl+S|保存项目|
|Ctrl+Shift+S|另存为|
|Ctrl+Z|撤销|
|Ctrl+Y|重做|
|Ctrl+F|搜索参数|
|Ctrl+G|生成代码|
|Del|删除选中参数|
|F2|重命名|
|Ctrl+D|复制参数|

### 4.2 右键菜单

**结构大纲右键菜单**:

```
添加子参数 (A)      →  基础类型  →  uint8/uint16/...
复制 (Ctrl+C)            字符类型  →  char/char[]
粘贴 (Ctrl+V)            枚举类型  →  enum
删除 (Del)               复杂类型  →  struct
重命名 (F2)      
─────────────────
保存为模板
导出为JSON
```

**画布卡片右键菜单**:

```
编辑属性
复制参数
删除参数
─────────────────
上移
下移
```

### 4.3 拖拽规则

- **类型库 → 画布**: 创建新参数卡片
- **画布内拖拽**: 调整参数顺序，显示蓝色插入线
- **大纲树拖拽**: 重组结构层级
- **拖拽限制**:
    - 不能将父节点拖入子节点（循环引用检测）
    - 基础类型不能包含子参数

### 4.4 校验机制

#### 实时校验 (输入时)

- 变量名不能为空
- 变量名不能重复（同级检测）
- 数值范围检查（uint16: 0-65535）
- 枚举项不能为空

#### 完整校验 (生成前)

- 所有参数必填项完整
- 无命名冲突
- 无循环依赖
- 类型兼容性检查

**校验结果显示**:

```
┌──────────────────────────────┐
│ 校验报告                     │
├──────────────────────────────┤
│ ✓ 通过: 25 项                │
│ ⚠ 警告: 3 项                 │
│   - Temperature 缺少单位     │
│   - GPS.Altitude 超出建议范围│
│ ✗ 错误: 1 项                 │
│   - Sensor 下存在重名参数    │
│                              │
│ [修复建议] [继续生成] [取消] │
└──────────────────────────────┘
```

---

## 五、技术实现要点

### 5.1 Qt 组件选型

|功能|推荐组件|备选方案|
|---|---|---|
|结构大纲|QTreeWidget|QTreeView + Model|
|主画布|QScrollArea + QVBoxLayout|QGraphicsView|
|属性面板|QFormLayout|QStackedWidget|
|代码编辑|QPlainTextEdit + Highlighter|QScintilla|
|JSON预览|QTextEdit + QSyntaxHighlighter|-|

### 5.2 数据模型设计

```cpp
class ParamModel : public QAbstractItemModel {
public:
    // 树形结构操作
    QModelIndex addParameter(const QModelIndex& parent, ParamMetadata data);
    bool removeParameter(const QModelIndex& index);
    bool moveParameter(const QModelIndex& from, const QModelIndex& to);
    
    // 数据访问
    ParamMetadata getParameter(const QModelIndex& index);
    void updateParameter(const QModelIndex& index, ParamMetadata data);
    
    // 序列化
    void saveToFile(const QString& path);
    bool loadFromFile(const QString& path);
    
signals:
    void parameterChanged();
    void validationError(const QString& message);
};
```

### 5.3 撤销/重做机制

使用 Qt Undo Framework:

```cpp
class AddParameterCommand : public QUndoCommand {
public:
    void undo() override {
        model->removeParameter(index);
    }
    void redo() override {
        index = model->addParameter(parent, data);
    }
private:
    ParamModel* model;
    QModelIndex parent;
    QModelIndex index;
    ParamMetadata data;
};

// 使用
QUndoStack undoStack;
undoStack.push(new AddParameterCommand(model, parent, data));
```

### 5.4 主题系统

使用 QSS (Qt Style Sheets):

```cpp
// 亮色主题
app.setStyleSheet(R"(
    QMainWindow { background: #F5F5F5; }
    QTreeWidget { background: white; border: 1px solid #E0E0E0; }
    QPushButton { background: #2196F3; color: white; border-radius: 4px; }
)");

// 暗色主题
app.setStyleSheet(R"(
    QMainWindow { background: #1E1E1E; }
    QTreeWidget { background: #252526; color: #CCCCCC; }
    QPushButton { background: #0E639C; color: white; }
)");
```

---

## 六、开发路线图

### MVP 阶段 (2-3 周)

**目标**: 核心功能可用，能完成基本的参数配置和代码生成

- [ ] 基础UI框架（三栏布局）
- [ ] 参数模型（支持基础类型 + 嵌套）
- [ ] 结构大纲树（增删改查）
- [ ] 属性编辑面板（表单验证）
- [ ] JSON 文件保存/加载
- [ ] C++ 代码生成器（基础模板）
- [ ] 简单校验机制

**验收标准**:

- 能创建包含3层嵌套的参数结构
- 生成可编译的 C++ 代码
- 保存并重新打开项目

### V1.0 阶段 (4-6 周)

**目标**: 完善用户体验，支持实际项目使用

- [ ] Python 代码生成器
- [ ] 模板系统（3-5个官方模板）
- [ ] 完整校验系统（依赖检查）
- [ ] 撤销/重做
- [ ] 搜索/过滤参数
- [ ] 暗黑主题
- [ ] 用户配置保存
- [ ] 帮助文档（内置）

**验收标准**:

- 通过10个真实仿真项目测试
- 用户无需文档即可上手

### V2.0 阶段 (长期)

**目标**: 高级功能和性能优化

- [ ] 增量代码生成
- [ ] 版本对比（配置diff）
- [ ] 批量导入（Excel/CSV）
- [ ] 插件系统（自定义生成器）
- [ ] 性能优化（1000+参数）
- [ ] 单元测试覆盖率 >80%

---

## 七、测试计划

### 7.1 单元测试

- 参数模型操作（增删改查）
- 类型转换（JSON ↔ 内存对象）
- 代码生成器（模板渲染）
- 校验逻辑（边界条件）

### 7.2 集成测试

- 完整流程：创建→编辑→保存→重新打开
- 复杂嵌套结构（5层+）
- 大规模参数（500+项）

### 7.3 用户测试

- 新手上手时间 < 10分钟
- 完成典型任务时间 < 5分钟
- 错误率 < 5%

---

## 八、风险与应对

|风险|可能性|影响|应对措施|
|---|---|---|---|
|Qt版本兼容性|中|高|最低支持 Qt 5.15，测试多版本|
|大规模参数性能|高|中|虚拟化列表、延迟加载|
|代码生成器扩展性|中|高|模板引擎解耦、插件架构|
|用户学习曲线|中|中|内置教程、示例项目|

---

## 九、附录

### 9.1 命名规范

- **类名**: PascalCase (ParamModel)
- **函数名**: camelCase (addParameter)
- **变量名**: camelCase (paramData)
- **常量**: UPPER_SNAKE_CASE (MAX_DEPTH)
- **文件名**: snake_case (param_model.cpp)

### 9.2 代码风格

- 使用 Qt 命名约定
- 指针使用驼峰命名 + Ptr 后缀
- 信号槽命名清晰（on_xxx_clicked）
- 注释使用 Doxygen 格式

### 9.3 资源清单

- **图标库**: Material Design Icons (SVG)
- **字体**: Roboto (英文), 思源黑体 (中文)
- **配色**: Material Design 调色板
- **文档**: Markdown + MkDocs

---

**最后更新**: 2025-10-11  
**文档版本**: v1.0  
**维护者**: [开发团队]

## 开发进展（2025-10-11 17.45 更新）

- 架构与构建
  - CMake + Qt 5.5.1（MSVC2015 x64）。顶部/底部固定高度，中间自适应，三栏布局。
  - 左侧改为 `QTabWidget`：模板/实例两页，互不干扰。

- 模板页（已完成）
  - 树 + 卡片联动，卡片可展开/折叠（结构递归）。
  - 右侧表单：名称、类型（含 enum/char[] 特殊项）、单位、默认值、描述。
  - 枚举编辑：
    - “编辑枚举项”对话框：支持名称 + 数值，添加/更新/删除、自动递增，保证后项值大于前项值。
    - “枚举默认值”下拉以“名称(值)”显示；切换时以名称作为真实默认值。
  - 校验：名称正则、同级重名、范围、枚举项与默认值一致性；类型切换为非 struct 自动清空 children。
  - 保存/打开 `.spe`：包含 `parameters` 与 `instances`；保存/生成前校验阻断。
  - 代码生成（模板）：每个 struct 输出 `.h/.cpp`，包含递归子结构支持。

- 实例页（已完成基础）
  - 实例树：新建/重命名/删除；右键“编辑值”。
  - 中间画布实例编辑器：按结构分组渲染；叶子字段可编辑。
    - enum 字段使用下拉；数值型带验证器；编辑完成即回写。
  - 生成实例代码：
    - 弹出“选择实例”对话框（默认全选）→ 生成 `Instances.h/.cpp`；
    - 递归收集并包含所有涉及 struct 的头文件，兼容 VS2015；实例变量使用聚合初始化与 lowerCamel 命名。
  - 生成实例 JSON 读写：已接线输出 `InstancesJson.cpp`（基于 `third_party/json/json.hpp` 单头）。

- 兼容性与注意事项
  - Windows936 代码页警告无伤大雅；建议源文件保存为 UTF-8。
  - 模板代码中使用 `QString`（需要 QtCore 头可见）。

## 下一步计划

- 生成面板整合
  - 在“生成”对话框中统一勾选：模板代码、实例代码、实例 JSON 读写；实例列表默认全选。
- 实例编辑器增强
  - enum 下拉显示“名称(值)”；数值型替换为 SpinBox/DoubleSpinBox（已部分完成）。
  - 组折叠/搜索、批量复制/粘贴、重置为模板默认值。
- 模板页快捷操作
  - struct 节点右键“基于模板创建实例”，自动切换到实例页并打开编辑器。
- 生成器增强
  - 若有 `enumValues` 则输出对应的 C++ enum/constexpr 映射（可选）。

## 使用指引（当前）

1. 模板编辑：
   - 在“模板”页选择字段，右侧表单修改后点“应用”；
   - 枚举：点“编辑枚举项”→ 设置名称与数值，支持“自动递增”；默认值在“枚举默认值”下拉选择。
2. 实例编辑：
   - 在“实例”页右键新建实例，选择类型并命名；
   - 中间编辑器直接修改 enum/数值/文本字段，自动回写。
3. 文件与生成：
   - 保存/打开 `.spe`（包含参数与实例）；
   - 生成模板代码（.h/.cpp）；
   - 生成实例：选择需生成的实例（默认全选），输出 `Instances.h/.cpp` 及 `InstancesJson.cpp`。