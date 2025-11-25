#ifndef JSON_IO_H
#define JSON_IO_H

#include <QString>
#include "core/param_types.h"

namespace SpeIO {

bool saveProject(const QString& path, const ParamMetadata& root);
bool loadProject(const QString& path, ParamMetadata& root);

// 扩展：同时保存/加载实例
bool saveProjectAll(const QString& path, const ParamMetadata& root, const QVector<InstanceMetadata>& instances);
bool loadProjectAll(const QString& path, ParamMetadata& root, QVector<InstanceMetadata>& instances);

}

#endif // JSON_IO_H

