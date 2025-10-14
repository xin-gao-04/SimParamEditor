#include "json_io.h"

#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static QJsonObject paramToJson(const ParamMetadata& p)
{
    QJsonObject o;
    o["name"] = p.name;
    o["type"] = paramTypeToString(p.type);
    if (!p.unit.isEmpty()) o["unit"] = p.unit;
    if (p.defaultValue.isValid()) o["default"] = QJsonValue::fromVariant(p.defaultValue);
    if (!p.description.isEmpty()) o["description"] = p.description;
    if (!p.enumItems.isEmpty()) {
        QJsonArray arr; for (const auto& it : p.enumItems) arr.append(it); o["enumItems"] = arr;
    }
    if (p.arraySize > 0) o["arraySize"] = p.arraySize;
    if (!p.children.isEmpty()) {
        QJsonArray arr;
        for (const auto& c : p.children) arr.append(paramToJson(c));
        o["children"] = arr;
    }
    return o;
}

static ParamMetadata jsonToParam(const QJsonObject& o)
{
    ParamMetadata p;
    p.name = o.value("name").toString();
    p.type = stringToParamType(o.value("type").toString());
    p.unit = o.value("unit").toString();
    p.defaultValue = o.value("default").toVariant();
    p.description = o.value("description").toString();
    p.arraySize = o.value("arraySize").toInt(0);
    if (o.contains("enumItems")) {
        for (const auto& v : o.value("enumItems").toArray()) p.enumItems.append(v.toString());
    }
    if (o.contains("children")) {
        for (const auto& v : o.value("children").toArray()) p.children.append(jsonToParam(v.toObject()));
    }
    return p;
}

bool SpeIO::saveProject(const QString& path, const ParamMetadata& root)
{
    QJsonObject rootObj;
    rootObj["version"] = "1.0";
    rootObj["projectName"] = root.name;
    QJsonArray params; params.append(paramToJson(root));
    rootObj["parameters"] = params;

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QJsonDocument doc(rootObj);
    f.write(doc.toJson(QJsonDocument::Indented));
    return f.commit();
}

bool SpeIO::loadProject(const QString& path, ParamMetadata& root)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const auto data = f.readAll();
    f.close();

    QJsonParseError err; QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    auto obj = doc.object();
    auto arr = obj.value("parameters").toArray();
    if (arr.isEmpty()) return false;
    root = jsonToParam(arr.first().toObject());
    return true;
}

// 实例序列化（简化）：仅输出 name/typePath/values（children 简单递归）
static QJsonObject instanceToJson(const InstanceMetadata& in)
{
    QJsonObject o; o["name"] = in.name; o["typePath"] = in.typePath;
    if (!in.values.isEmpty()) {
        QJsonObject vo; for (auto it = in.values.constBegin(); it != in.values.constEnd(); ++it) vo[it.key()] = QJsonValue::fromVariant(it.value());
        o["values"] = vo;
    }
    if (!in.children.isEmpty()) {
        QJsonArray arr; for (const auto& c: in.children) arr.append(instanceToJson(c)); o["children"] = arr;
    }
    return o;
}

static InstanceMetadata jsonToInstance(const QJsonObject& o)
{
    InstanceMetadata in; in.name = o.value("name").toString(); in.typePath = o.value("typePath").toString();
    if (o.contains("values")) {
        const auto vo = o.value("values").toObject();
        for (auto it = vo.begin(); it != vo.end(); ++it) in.values[it.key()] = it.value().toVariant();
    }
    if (o.contains("children")) {
        for (const auto& v : o.value("children").toArray()) in.children.append(jsonToInstance(v.toObject()));
    }
    return in;
}

bool SpeIO::saveProjectAll(const QString& path, const ParamMetadata& root, const QVector<InstanceMetadata>& instances)
{
    QJsonObject rootObj;
    rootObj["version"] = "1.0";
    rootObj["projectName"] = root.name;
    QJsonArray params; params.append(paramToJson(root)); rootObj["parameters"] = params;
    QJsonArray instArr; for (const auto& in : instances) instArr.append(instanceToJson(in)); rootObj["instances"] = instArr;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QJsonDocument doc(rootObj); f.write(doc.toJson(QJsonDocument::Indented));
    return f.commit();
}

bool SpeIO::loadProjectAll(const QString& path, ParamMetadata& root, QVector<InstanceMetadata>& instances)
{
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return false;
    auto data = f.readAll(); f.close();
    QJsonParseError err; auto doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    auto obj = doc.object(); auto arr = obj.value("parameters").toArray(); if (arr.isEmpty()) return false;
    root = jsonToParam(arr.first().toObject());
    instances.clear();
    for (const auto& v : obj.value("instances").toArray()) instances.append(jsonToInstance(v.toObject()));
    return true;
}


