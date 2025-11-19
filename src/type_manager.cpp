#include "type_manager.h"

TypeManager& TypeManager::instance() {
    static TypeManager inst;
    return inst;
}

bool TypeManager::registerType(const QString& name, const ParamMetadata& meta) {
    if (m_types.contains(name)) {
        return false;
    }
    // 确保元数据的名称与注册名称一致
    ParamMetadata newMeta = meta;
    newMeta.name = name;
    
    m_types.insert(name, newMeta);
    emit typeRegistered(name);
    return true;
}

bool TypeManager::updateType(const QString& name, const ParamMetadata& meta) {
    if (!m_types.contains(name)) {
        return false;
    }
    // 保持名称一致性
    ParamMetadata newMeta = meta;
    newMeta.name = name;

    m_types[name] = newMeta;
    emit typeUpdated(name);
    return true;
}

const ParamMetadata* TypeManager::getType(const QString& name) const {
    if (!m_types.contains(name)) {
        return nullptr;
    }
    return &m_types[name];
}

bool TypeManager::hasType(const QString& name) const {
    return m_types.contains(name);
}

QStringList TypeManager::getAllTypeNames() const {
    return m_types.keys();
}

QStringList TypeManager::getTypeNames(ParamType type) const {
    QStringList result;
    for (auto it = m_types.begin(); it != m_types.end(); ++it) {
        if (it.value().type == type) {
            result.append(it.key());
        }
    }
    return result;
}

bool TypeManager::removeType(const QString& name) {
    if (m_types.remove(name) > 0) {
        emit typeRemoved(name);
        return true;
    }
    return false;
}

void TypeManager::clear() {
    m_types.clear();
    // 如果需要，可以发送 clear 信号，或者逐个发送 removed 信号
    // 这里暂时简单处理
}

