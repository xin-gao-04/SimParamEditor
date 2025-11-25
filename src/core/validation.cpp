#include "validation.h"
#include "type_manager.h"

#include <QRegExp>
#include <QSet>

static void validateNode(const ParamMetadata& node, const QString& path, ValidationReport& out)
{
    // name rule
    static QRegExp re("^[A-Za-z_][A-Za-z0-9_]*$");
    if (!re.exactMatch(node.name)) {
        out.issues.push_back({ ValidationIssue::Error, path, QString("非法名称: %1").arg(node.name) });
    }

    // children allowance
    if (!canHaveChildren(node.type) && !node.children.isEmpty()) {
        out.issues.push_back({ ValidationIssue::Error, path, ("该类型不允许包含子项") });
    }

    // type specific
    if (!node.typeName.isEmpty()) {
        if (!TypeManager::instance().hasType(node.typeName)) {
            out.issues.push_back({ ValidationIssue::Error, path, QString("引用了不存在的类型: %1").arg(node.typeName) });
        }
        // 如果引用有效，通常不需要再次校验引用的定义本身（假设定义时已校验）
        // 但需要确保本地没有冲突的 children 定义（TypeManager 模式下 children 应为空）
        if (!node.children.isEmpty()) {
             out.issues.push_back({ ValidationIssue::Warning, path, QString("引用类型节点不应包含本地子节点 (将被忽略)") });
        }
    } else if (node.type == ParamType::ENUM) {
        if (node.enumItems.isEmpty()) {
            out.issues.push_back({ ValidationIssue::Error, path, ("枚举项不能为空") });
        }
        if (node.defaultValue.isValid() && !node.enumItems.contains(node.defaultValue.toString())) {
            out.issues.push_back({ ValidationIssue::Error, path, ("默认值不在枚举项中") });
        }
    } else if (node.type == ParamType::CHAR_ARRAY) {
        if (node.arraySize <= 0) {
            out.issues.push_back({ ValidationIssue::Error, path, ("char[] 长度必须 > 0") });
        }
    } else if (isNumericType(node.type)) {
        if (node.defaultValue.isValid()) {
            if (node.type == ParamType::FLOAT || node.type == ParamType::DOUBLE) {
                // no strict bound
            } else {
                const auto r = integerRange(node.type);
                bool ok = false; qint64 v = node.defaultValue.toLongLong(&ok);
                if (!ok || (!r.first && !r.second)) {
                    // unexpected, ignore
                } else {
                    if (v < r.first || v > r.second) {
                        out.issues.push_back({ ValidationIssue::Error, path, ("默认值超出范围") });
                    }
                }
            }
        }
    }

    // duplicates among siblings
    if (!node.children.isEmpty()) {
        QSet<QString> names;
        for (int i = 0; i < node.children.size(); ++i) {
            const auto& c = node.children[i];
            if (names.contains(c.name)) {
                out.issues.push_back({ ValidationIssue::Error, path, QString("同级存在重名: %1").arg(c.name) });
            } else {
                names.insert(c.name);
            }
        }
    }

    // recurse
    for (int i = 0; i < node.children.size(); ++i) {
        const auto& c = node.children[i];
        const QString childPath = path + "/" + c.name;
        validateNode(c, childPath, out);
    }
}

ValidationReport validateProject(const ParamMetadata& root)
{
    ValidationReport r;
    validateNode(root, root.name.isEmpty()? QString("/") : QString("/") + root.name, r);
    return r;
}
