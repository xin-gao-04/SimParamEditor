#include "cpp_generator.h"
#include "../type_manager.h"

#include <QDir>
#include <QSaveFile>
#include <QTextStream>
#include <QSet>

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

    bool ok = true;
    
    // 1. Generate all registered types
    QStringList typeNames = TypeManager::instance().getAllTypeNames();
    for (const QString& tn : typeNames) {
        const ParamMetadata* meta = TypeManager::instance().getType(tn);
        if (meta && meta->type == ParamType::STRUCT) {
            ok = generateStruct(*meta, outDir) && ok;
        }
    }

    // 2. Generate Root Config Struct (if it's not just a reference)
    if (root.type == ParamType::STRUCT && root.typeName.isEmpty()) {
        ok = generateStruct(root, outDir) && ok;
    }
    
    return ok;
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
            // Leaf field
            QVariant val = inst.values.value(currentKey);
            // fallback to default
            if (!val.isValid()) val = field.defaultValue;
            
            QString vStr = val.toString();
            
            if (field.type == ParamType::CHAR_ARRAY && field.arraySize > 0) {
                s += "\"" + vStr + "\"";
            } else if (field.type == ParamType::ENUM) {
                 // 枚举值通常是 INT，如果是字符串名称需要转换？
                 // 现有逻辑假设 vStr 是数值或者直接可用的字面量
                 // 如果 val 是字符串且是 enumItem 名，可能需要转为数值或 Enum::Member
                 // 这里简化：如果值是空的，用0
                 s += vStr.isEmpty()? QString("0") : vStr; 
            } else if (field.type == ParamType::FLOAT || field.type == ParamType::DOUBLE) {
                s += vStr.isEmpty()? QString("0.0f") : vStr;
            } else {
                s += vStr.isEmpty()? QString("0") : vStr;
            }
        }
    }
    s += "}";
    return s;
}

bool CppGenerator::writeInstancesUnit(const ParamMetadata& root, const QVector<InstanceMetadata>& instances, const QString& outDir)
{
    if (instances.isEmpty()) return true;
    QSaveFile hf(outDir + "/Instances.h");
    if (!hf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream hout(&hf); hout.setCodec("UTF-8");
    hout << "// Auto-generated by SimParamEditor\n#pragma once\n\n";
    // include：收集所有涉及到的 struct 以及其子 struct，确保复合结构的头文件全部包含
    QSet<QString> typeNames;
    struct Walker {
        static void collect(const ParamMetadata& t, QSet<QString>& names) {
            if (t.type != ParamType::STRUCT) return;
            names.insert(t.name);
            for (int i = 0; i < t.children.size(); ++i) {
                const ParamMetadata& c = t.children.at(i);
                if (c.type == ParamType::STRUCT) collect(c, names);
            }
        }
    };
    for (const auto& inst : instances) {
        const ParamMetadata* tn = findTypeNodeByPath(root, inst.typePath);
        if (tn) Walker::collect(*tn, typeNames);
    }
    for (const QString& n : typeNames) hout << "#include \"" << sanitize(n) << ".h\"\n";
    hout << "\n";
    if (!hf.commit()) return false;

    QSaveFile sf(outDir + "/Instances.cpp");
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream sout(&sf); sout.setCodec("UTF-8");
    sout << "// Auto-generated by SimParamEditor\n";
    sout << "#include \"Instances.h\"\n";
    sout << "#include <string>\n";
    // json head-only（用户将提供 third_party/json/json.hpp）
    sout << "#include \"json.hpp\"\n\n";
    sout << "using nlohmann::json;\n\n";
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
        if (c.type == ParamType::STRUCT) {
            out << ind << "{ json child;\n";
            renderToJson(out, c, varName + "." + sanitize(c.name), "child", indent + 2);
            out << ind << jsonVar << "[\"" << sanitize(c.name) << "\"] = child; }\n";
        } else {
            out << ind << jsonVar << "[\"" << sanitize(c.name) << "\"] = " << varName << "." << sanitize(c.name) << ";\n";
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
        if (c.type == ParamType::STRUCT) {
            out << ind << "if(" << jsonVar << ".contains(\"" << sanitize(c.name) << "\")) {\n";
            out << ind << "  const json& child = " << jsonVar << "[\"" << sanitize(c.name) << "\"];\n";
            renderFromJson(out, c, varName + "." + sanitize(c.name), "child", indent + 2);
            out << ind << "}\n";
        } else {
            out << ind << "if(" << jsonVar << ".contains(\"" << sanitize(c.name) << "\")) " << varName << "." << sanitize(c.name) << " = " << jsonVar << "[\"" << sanitize(c.name) << "\"];\n";
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
    out << "#include \"json.hpp\"\n\n";
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


