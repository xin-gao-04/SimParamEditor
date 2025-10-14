#include "validation.h"

#include <QRegExp>
#include <QSet>

static void validateNode(const ParamMetadata& node, const QString& path, ValidationReport& out)
{
    // name rule
    static QRegExp re("^[A-Za-z_][A-Za-z0-9_]*$");
    if (!re.exactMatch(node.name)) {
        out.issues.push_back({ ValidationIssue::Error, path, QString::fromUtf8(u8"\u975e\u6cd5\u540d\u79f0: %1").arg(node.name) });
    }

    // children allowance
    if (!canHaveChildren(node.type) && !node.children.isEmpty()) {
        out.issues.push_back({ ValidationIssue::Error, path, QString::fromUtf8(u8"\u8be5\u7c7b\u578b\u4e0d\u5141\u8bb8\u5305\u542b\u5b50\u9879") });
    }

    // type specific
    if (node.type == ParamType::ENUM) {
        if (node.enumItems.isEmpty()) {
            out.issues.push_back({ ValidationIssue::Error, path, QString::fromUtf8(u8"\u679a\u4e3e\u9879\u4e0d\u80fd\u4e3a\u7a7a") });
        }
        if (node.defaultValue.isValid() && !node.enumItems.contains(node.defaultValue.toString())) {
            out.issues.push_back({ ValidationIssue::Error, path, QString::fromUtf8(u8"\u9ed8\u8ba4\u503c\u4e0d\u5728\u679a\u4e3e\u9879\u4e2d") });
        }
    } else if (node.type == ParamType::CHAR_ARRAY) {
        if (node.arraySize <= 0) {
            out.issues.push_back({ ValidationIssue::Error, path, QString::fromUtf8(u8"char[] \u957f\u5ea6\u5fc5\u987b > 0") });
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
                        out.issues.push_back({ ValidationIssue::Error, path, QString::fromUtf8(u8"\u9ed8\u8ba4\u503c\u8d85\u51fa\u8303\u56f4") });
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
                out.issues.push_back({ ValidationIssue::Error, path, QString::fromUtf8(u8"\u540c\u7ea7\u5b58\u5728\u91cd\u540d: %1").arg(c.name) });
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


