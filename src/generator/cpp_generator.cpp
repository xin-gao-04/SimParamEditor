#include "cpp_generator.h"
#include "core/type_manager.h"

#include <QDir>
#include <QSaveFile>
#include <QTextStream>
#include <QSet>
#include <functional>

QString CppGenerator::sanitize(const QString& name)
{
    QString s = name;
    if (s.isEmpty()) s = "Unnamed";
    // simple sanitize: replace spaces and invalid chars with '_'
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s[i];
        if (!(c.isLetterOrNumber() || c == '_' )) s[i] = '_';
    }
    if (s[0].isDigit()) s.prepend('_');
    return s;
}

QString CppGenerator::cppTypeFor(ParamType t)
{
    switch (t) {
    case ParamType::UINT8: return "uint8_t";    case ParamType::UINT16: return "uint16_t";  case ParamType::UINT32: return "uint32_t";
    case ParamType::INT8:  return "int8_t";     case ParamType::INT16:  return "int16_t";   case ParamType::INT32:  return "int32_t";
    case ParamType::FLOAT: return "float";      case ParamType::DOUBLE: return "double";
    case ParamType::CHAR:  return "char";       case ParamType::CHAR_ARRAY: return "char"; // array handled separately
    default: return QString();
    }
}

QString CppGenerator::toLowerCamel(const QString& s)
{
    if (s.isEmpty()) return s;
    QString out = sanitize(s);
    out[0] = out[0].toLower();
    return out;
}

bool CppGenerator::generate(const ParamMetadata& root, const QString& outDir)
{
    QDir dir(outDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) return false;
    }
    // 统一写入一个 Types.h，避免碎片化的多头文件
    return writeTypesUnit(root, outDir);
}

bool CppGenerator::generateInstances(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir)
{
    return writeInstancesUnit(root, instances, outDir);
}

bool CppGenerator::generateInstancesJson(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir)
{
    return writeInstancesJsonUnit(root, instances, outDir);
}

static const ParamMetadata* findTypeNodeByPath(const ParamMetadata& root, const QString& path)
{
    // path like /Root/A/B
    QStringList parts = path.split('/', QString::SkipEmptyParts);
    if (parts.isEmpty()) return nullptr;
    if (parts.first() != root.name) return nullptr;
    const ParamMetadata* cur = &root;
    for (int i = 1; i < parts.size(); ++i) {
        // 如果当前节点是类型引用，先切换到定义
        if (!cur->typeName.isEmpty()) {
             const ParamMetadata* def = TypeManager::instance().getType(cur->typeName);
             if (def) cur = def;
        }
        
        bool ok=false;
        for (const auto& c : cur->children) { 
            if (c.name == parts[i]) { 
                cur = &c; 
                ok=true; 
                break; 
            } 
        }
        if (!ok) return nullptr;
    }
    
    // 最终找到节点后，如果是引用，也返回定义
    if (!cur->typeName.isEmpty()) {
        const ParamMetadata* def = TypeManager::instance().getType(cur->typeName);
        if (def) return def;
    }
    
    return cur;
}

QString CppGenerator::renderAggregateInit(const ParamMetadata& typeNode, const InstanceMetadata& inst, const QString& prefix)
{
    QString s("{");
    bool first = true;
    
    // 解析 typeNode (如果是引用)
    const ParamMetadata* def = &typeNode;
    if (!typeNode.typeName.isEmpty()) {
        const ParamMetadata* t = TypeManager::instance().getType(typeNode.typeName);
        if (t) def = t;
    }
    
    for (const auto& field : def->children) {
        if (!first) s += ", "; first = false;
        QString currentKey = prefix.isEmpty() ? field.name : (prefix + "/" + field.name);

        if (field.type == ParamType::STRUCT) {
            s += renderAggregateInit(field, inst, currentKey);
        } else {
            // 叶子字段：从实例 Diff 或默认值中取值
            QVariant val = inst.values.value(currentKey);
            if (!val.isValid()) val = field.defaultValue;

            if (field.type == ParamType::ENUM) {
                // 枚举：根据名称或数值转换为枚举常量
                QString enumTypeName;
                if (!field.typeName.isEmpty()) {
                    enumTypeName = sanitize(field.typeName);   // 全局枚举
                } else {
                    // 本地枚举：类型名直接使用字段名（与 writeTypesUnit 中保持一致）
                    enumTypeName = sanitize(field.name);
                }

                const ParamMetadata* enumDef = nullptr;
                if (!field.typeName.isEmpty()) {
                    enumDef = TypeManager::instance().getType(field.typeName);
                } else {
                    enumDef = &field;
                }

                QString expr = enumTypeName + "(0)";
                if (enumDef) {
                    QString name = val.toString();
                    int idx = enumDef->enumItems.indexOf(name);
                    if (idx >= 0) {
                        QString itemName = sanitize(enumDef->enumItems.at(idx));
                        expr = enumTypeName + "::" + itemName;
                    } else {
                        bool okNum = false;
                        int num = val.toInt(&okNum);
                        if (!okNum) num = 0;
                        expr = "static_cast<" + enumTypeName + ">(" + QString::number(num) + ")";
                    }
                }
                s += expr;
            } else {
                QString vStr = val.toString();
                if (field.type == ParamType::CHAR_ARRAY && field.arraySize > 0) {
                    s += "\"" + vStr + "\"";
                } else if (field.type == ParamType::FLOAT || field.type == ParamType::DOUBLE) {
                    s += vStr.isEmpty()? QString("0.0f") : vStr;
                } else {
                    s += vStr.isEmpty()? QString("0") : vStr;
                }
            }
        }
    }
    s += "}";
    return s;
}

bool CppGenerator::writeTypesUnit(const ParamMetadata& root, const QString& outDir)
{
    QSaveFile hf(outDir + "/Types.h");
    if (!hf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream out(&hf); out.setCodec("UTF-8");

    out << "// Auto-generated by SimParamEditor\n";
    out << "#pragma once\n\n";
    out << "#include <cstdint>\n";
    out << "\n";

    // 收集所有 struct / enum 定义
    QVector<const ParamMetadata*> structDefs;
    QSet<const ParamMetadata*>    structSet;
    QVector<const ParamMetadata*> globalEnums;

    // 递归收集结构体定义：先收集内联子 struct，再收集自身，保证依赖在前
    std::function<void(const ParamMetadata*)> collectStructs;
    collectStructs = [&](const ParamMetadata* node) {
        if (!node) return;
        // 先递归收集内联子 struct（没有 typeName 的 struct 字段）
        for (const auto& c : node->children) {
            if (c.type == ParamType::STRUCT && c.typeName.isEmpty()) {
                collectStructs(&c);
            }
        }
        // 再把当前节点加入列表
        if (node->type == ParamType::STRUCT && !structSet.contains(node)) {
            structDefs.append(node);
            structSet.insert(node);
        }
    };

    // 全局类型管理器中的类型：struct / enum
    QStringList typeNames = TypeManager::instance().getAllTypeNames();
    for (const QString& tn : typeNames) {
        const ParamMetadata* meta = TypeManager::instance().getType(tn);
        if (!meta) continue;
        if (meta->type == ParamType::STRUCT) {
            collectStructs(meta);
        } else if (meta->type == ParamType::ENUM) {
            globalEnums.append(meta);
        }
    }

    // 根结构如果是 struct 且不是引用，也一并纳入（含其内联子 struct）
    if (root.type == ParamType::STRUCT && root.typeName.isEmpty()) {
        collectStructs(&root);
    }

    // 为本地枚举（未引用全局类型的 enum 字段）生成独立枚举类型名
    QMap<const ParamMetadata*, QString> localEnumNames;
    for (const ParamMetadata* st : structDefs) {
        for (const auto& c : st->children) {
            if (c.type == ParamType::ENUM && c.typeName.isEmpty()) {
                // 本地枚举：类型名直接使用字段名，避免额外结构体前缀
                QString enumType = sanitize(c.name);
                localEnumNames.insert(&c, enumType);
            }
        }
    }

    // 先输出全局枚举
    for (const ParamMetadata* e : globalEnums) {
        QString enumName = sanitize(e->name);
        out << "enum class " << enumName << " : int32_t {\n";
        int currentVal = 0;
        for (int i = 0; i < e->enumItems.size(); ++i) {
            if (i < e->enumValues.size())
                currentVal = e->enumValues.at(i);
            QString itemName = sanitize(e->enumItems.at(i));
            out << "    " << itemName << " = " << currentVal;
            if (i + 1 < e->enumItems.size()) out << ",";
            out << "\n";
            ++currentVal;
        }
        out << "};\n\n";
    }

    // 输出本地枚举
    for (auto it = localEnumNames.constBegin(); it != localEnumNames.constEnd(); ++it) {
        const ParamMetadata* metaEnum = it.key();   // 字段上的枚举定义
        const QString enumName = it.value();
        out << "enum class " << enumName << " : int32_t {\n";
        int currentVal = 0;
        for (int i = 0; i < metaEnum->enumItems.size(); ++i) {
            if (i < metaEnum->enumValues.size())
                currentVal = metaEnum->enumValues.at(i);
            QString itemName = sanitize(metaEnum->enumItems.at(i));
            out << "    " << itemName << " = " << currentVal;
            if (i + 1 < metaEnum->enumItems.size()) out << ",";
            out << "\n";
            ++currentVal;
        }
        out << "};\n\n";
    }

    // 再输出所有结构体
    for (const ParamMetadata* st : structDefs) {
        const QString structName = sanitize(st->name);
        out << "struct " << structName << " {\n";
        for (const auto& c : st->children) {
            if (c.type == ParamType::STRUCT) {
                // struct 字段：优先使用 typeName 指向的全局结构类型
                QString type = c.typeName.isEmpty() ? sanitize(c.name) : sanitize(c.typeName);
                out << "    " << type << " " << sanitize(c.name) << ";\n";
            } else if (c.type == ParamType::CHAR_ARRAY && c.arraySize > 0) {
                out << "    char " << sanitize(c.name) << "[" << c.arraySize << "];\n";
            } else if (c.type == ParamType::ENUM) {
                QString enumType;
                if (!c.typeName.isEmpty()) {
                    enumType = sanitize(c.typeName);              // 引用全局枚举
                } else {
                    enumType = localEnumNames.value(&c, "int32_t"); // 本地枚举
                }
                out << "    " << enumType << " " << sanitize(c.name) << ";\n";
            } else {
                const QString ct = cppTypeFor(c.type);
                if (!ct.isEmpty())
                    out << "    " << ct << " " << sanitize(c.name) << ";\n";
            }
        }
        out << "};\n\n";
    }

    if (!hf.commit()) return false;
    return true;
}

bool CppGenerator::writeInstancesUnit(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir)
{
    if (instances.isEmpty()) return true;
    QSaveFile hf(outDir + "/Instances.h");
    if (!hf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream hout(&hf); hout.setCodec("UTF-8");
    hout << "// Auto-generated by SimParamEditor\n#pragma once\n\n";
    // 统一从 Types.h 引入所有类型定义
    hout << "#include \"Types.h\"\n\n";
    if (!hf.commit()) return false;

    QSaveFile sf(outDir + "/Instances.cpp");
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream sout(&sf); sout.setCodec("UTF-8");
    sout << "// Auto-generated by SimParamEditor\n";
    sout << "#include \"Instances.h\"\n\n";
    for (const auto& inst : instances) {
        const ParamMetadata* tn = findTypeNodeByPath(root, inst.typePath);
        if (!tn) continue;
        const QString typeName = sanitize(tn->name);
        const QString instName = toLowerCamel(inst.name);
        sout << typeName << " " << instName << " = " << renderAggregateInit(*tn, inst) << ";\n";
    }

    // 预留：生成 to_json/from_json 的简化示例（仅聚合 -> json 原型）
    sout << "\n// TODO: generate per-type to_json/from_json if needed.\n";
    return sf.commit();
}

void CppGenerator::renderToJson(QTextStream& out, const ParamMetadata& typeNode, const QString& varName, const QString& jsonVar, int indent)
{
    const QString ind(indent, ' ');
    out << ind << jsonVar << " = json::object();\n";
    
    const ParamMetadata* def = &typeNode;
    if (!typeNode.typeName.isEmpty()) {
        const ParamMetadata* t = TypeManager::instance().getType(typeNode.typeName);
        if (t) def = t;
    }

    for (int i = 0; i < def->children.size(); ++i) {
        const ParamMetadata& c = def->children.at(i);
        const QString fieldName = sanitize(c.name);
        const QString fullVar   = varName + "." + fieldName;

        if (c.type == ParamType::STRUCT) {
            out << ind << "{ json child;\n";
            renderToJson(out, c, fullVar, "child", indent + 2);
            out << ind << jsonVar << "[\"" << fieldName << "\"] = child; }\n";
        } else if (c.type == ParamType::ENUM) {
            // 枚举：以整型方式写入，避免直接序列化 enum class 失败
            out << ind << jsonVar << "[\"" << fieldName << "\"] = static_cast<int>(" << fullVar << ");\n";
        } else {
            out << ind << jsonVar << "[\"" << fieldName << "\"] = " << fullVar << ";\n";
        }
    }
}

void CppGenerator::renderFromJson(QTextStream& out, const ParamMetadata& typeNode, const QString& varName, const QString& jsonVar, int indent)
{
    const QString ind(indent, ' ');
    
    const ParamMetadata* def = &typeNode;
    if (!typeNode.typeName.isEmpty()) {
        const ParamMetadata* t = TypeManager::instance().getType(typeNode.typeName);
        if (t) def = t;
    }

    for (int i = 0; i < def->children.size(); ++i) {
        const ParamMetadata& c = def->children.at(i);
        const QString fieldName = sanitize(c.name);
        const QString fullVar   = varName + "." + fieldName;

        if (c.type == ParamType::STRUCT) {
            out << ind << "if(" << jsonVar << ".contains(\"" << fieldName << "\")) {\n";
            out << ind << "  const json& child = " << jsonVar << "[\"" << fieldName << "\"];\n";
            renderFromJson(out, c, fullVar, "child", indent + 2);
            out << ind << "}\n";
        } else if (c.type == ParamType::ENUM) {
            // 枚举：从整型还原为枚举类型
            QString enumTypeName;
            if (!c.typeName.isEmpty()) {
                enumTypeName = sanitize(c.typeName);           // 全局枚举
            } else {
                // 本地枚举：类型名直接使用字段名（与 writeTypesUnit 中保持一致）
                enumTypeName = sanitize(c.name);
            }
            out << ind << "if(" << jsonVar << ".contains(\"" << fieldName << "\")) "
                << fullVar << " = static_cast<" << enumTypeName << ">("
                << jsonVar << "[\"" << fieldName << "\"].get<int>());\n";
        } else {
            out << ind << "if(" << jsonVar << ".contains(\"" << fieldName << "\")) "
                << fullVar << " = " << jsonVar << "[\"" << fieldName << "\"];\n";
        }
    }
}

bool CppGenerator::writeInstancesJsonUnit(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir)
{
    if (instances.isEmpty()) return true;
    QSaveFile sf(outDir + "/InstancesJson.cpp");
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream out(&sf); out.setCodec("UTF-8");
    out << "// Auto-generated by SimParamEditor\n";
    out << "#include \"Instances.h\"\n";
    out << "#include \"third_party/json/json.hpp\"\n\n";
    out << "using nlohmann::json;\n\n";
    // 为每个实例生成 to_json/from_json 函数
    for (int i = 0; i < instances.size(); ++i) {
        const InstanceMetadata& inst = instances.at(i);
        const ParamMetadata* tn = findTypeNodeByPath(root, inst.typePath);
        if (!tn) continue;
        const QString typeName = sanitize(tn->name);
        const QString instName = toLowerCamel(inst.name);
        out << "json toJson_" << instName << "(const " << typeName << "& v) { json j;\n";
        renderToJson(out, *tn, "v", "j", 2);
        out << "  return j; }\n";
        out << typeName << " fromJson_" << instName << "(const json& j) { " << typeName << " v{};\n";
        renderFromJson(out, *tn, "v", "j", 2);
        out << "  return v; }\n\n";
    }
    return sf.commit();
}

bool CppGenerator::generateStruct(const ParamMetadata& node, const QString& outDir)
{
    const QString structName = sanitize(node.name);
    const QString headerName = structName + ".h";
    const QString sourceName = structName + ".cpp";

    // header
    QSaveFile hf(outDir + "/" + headerName);
    if (!hf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream hout(&hf);
    hout.setCodec("UTF-8");
    hout << "// Auto-generated by SimParamEditor\n";
    hout << "#pragma once\n";
    hout << "#include <cstdint>\n";
    hout << "#include <QString>\n";
    
    // 收集依赖并 include
    QSet<QString> includes;
    for (const auto& c : node.children) {
        if (c.type == ParamType::STRUCT && !c.typeName.isEmpty()) {
            includes.insert(sanitize(c.typeName));
        }
    }
    for (const QString& inc : includes) {
        hout << "#include \"" << inc << ".h\"\n";
    }
    hout << "\n";

    hout << "struct " << structName << " {\n";
    for (const auto& c : node.children) {
        if (c.type == ParamType::STRUCT) {
            // 如果使用了 typeName，使用该类型；否则假设是嵌套定义（不推荐但兼容）
            QString type = c.typeName.isEmpty() ? sanitize(c.name) : sanitize(c.typeName);
            hout << "    " << type << " " << sanitize(c.name) << ";\n"; 
        } else if (c.type == ParamType::CHAR_ARRAY && c.arraySize > 0) {
            hout << "    char " << sanitize(c.name) << "[" << c.arraySize << "];\n";
        } else {
            const QString ct = cppTypeFor(c.type);
            if (!ct.isEmpty())
                hout << "    " << ct << " " << sanitize(c.name) << ";\n";
        }
    }
    hout << "};\n";
    if (!hf.commit()) return false;

    // source (empty for now, placeholder)
    QSaveFile sf(outDir + "/" + sourceName);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream sout(&sf);
    sout.setCodec("UTF-8");
    sout << "// Auto-generated by SimParamEditor\n";
    sout << "#include \"" << headerName << "\"\n";
    if (!sf.commit()) return false;

    return true;
}

