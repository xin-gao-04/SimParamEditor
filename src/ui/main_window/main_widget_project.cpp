#include "ui/main_window/main_widget.h"

#include "core/json_io.h"
#include "core/type_manager.h"
#include "core/validation.h"
#include "generator/cpp_generator.h"
#include "ui/dialogs/instances_select_dialog.h"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

void MainWidget::buildSampleRoot()
{
    TypeManager::instance().clear();
    rootParam.name = "PayloadConfig";
    rootParam.type = ParamType::STRUCT;

    ParamMetadata sensor;
    sensor.name = "SensorModule";
    sensor.type = ParamType::STRUCT;
    ParamMetadata temperature;
    temperature.name = "Temperature";
    temperature.type = ParamType::UINT16;
    temperature.unit = QStringLiteral("°C");
    temperature.defaultValue = 25;
    ParamMetadata humidity;
    humidity.name = "Humidity";
    humidity.type = ParamType::UINT8;
    humidity.unit = "%";
    humidity.defaultValue = 60;
    ParamMetadata gps;
    gps.name = "GPSModule";
    gps.type = ParamType::STRUCT;
    ParamMetadata lat;
    lat.name = "Latitude";
    lat.type = ParamType::FLOAT;
    ParamMetadata lon;
    lon.name = "Longitude";
    lon.type = ParamType::FLOAT;
    ParamMetadata alt;
    alt.name = "Altitude";
    alt.type = ParamType::UINT16;
    alt.unit = "m";
    gps.children = { lat, lon, alt };
    sensor.children = { temperature, humidity, gps };

    ParamMetadata comm;
    comm.name = "CommunicationModule";
    comm.type = ParamType::STRUCT;
    ParamMetadata baud;
    baud.name = "Baudrate";
    baud.type = ParamType::UINT32;
    ParamMetadata proto;
    proto.name = "Protocol";
    proto.type = ParamType::ENUM;
    proto.enumItems = (QStringList() << "UART" << "SPI" << "I2C");
    proto.defaultValue = "UART";
    comm.children = { baud, proto };

    rootParam.children = { sensor, comm };

    instances.clear();
    InstanceMetadata radar1;
    radar1.name = "Radar1";
    radar1.typePath = "/" + rootParam.name + "/SensorModule";
    radar1.values.insert("Temperature", 25);
    radar1.values.insert("Humidity", 60);
    radar1.values.insert("GPSModule/Latitude", QString("39.90f"));
    radar1.values.insert("GPSModule/Longitude", QString("116.40f"));
    radar1.values.insert("GPSModule/Altitude", 50);
    instances.append(radar1);

    rebuildTreeFromModel();
    refreshCenterCanvas();
    rebuildInstancesTree();
    updateValidationStatus();
}

void MainWidget::onNewProjectClicked()
{
    buildSampleRoot();
    QMessageBox::information(this, QStringLiteral("新建"), QStringLiteral("已创建示例项目结构"));
}

void MainWidget::onOpenClicked()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开项目"), lastProjectPath, "SimParamEditor (*.spe)");
    if (path.isEmpty()) return;
    ParamMetadata temp;
    QVector<InstanceMetadata> insts;
    if (SpeIO::loadProjectAll(path, temp, insts)) {
        rootParam = temp;
        instances = insts;
        lastProjectPath = path;
        QMessageBox::information(this, QStringLiteral("打开项目"), QStringLiteral("加载成功"));
        rebuildTreeFromModel();
        rebuildInstancesTree();
        refreshCenterCanvas();
        updateValidationStatus();
    } else {
        QMessageBox::warning(this, QStringLiteral("打开项目"), QStringLiteral("加载失败"));
    }
}

void MainWidget::onSaveClicked()
{
    QString path = lastProjectPath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, QStringLiteral("保存项目"), QDir::homePath() + "/project.spe", "SimParamEditor (*.spe)");
        if (path.isEmpty()) return;
    }
    {
        auto report = validateProject(rootParam);
        if (report.hasError()) {
            showValidationReportDialog();
            QMessageBox::warning(this, QStringLiteral("保存阻止"), QStringLiteral("存在错误，请修复后再保存"));
            return;
        }
    }
    if (SpeIO::saveProjectAll(path, rootParam, instances)) {
        lastProjectPath = path;
        QMessageBox::information(this, QStringLiteral("保存项目"), QStringLiteral("保存成功"));
    } else {
        QMessageBox::warning(this, QStringLiteral("保存项目"), QStringLiteral("保存失败"));
    }
}

void MainWidget::onGenerateClicked()
{
    const QString out = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"), lastOutputDir.isEmpty() ? QDir::homePath() : lastOutputDir);
    if (out.isEmpty()) return;
    {
        auto report = validateProject(rootParam);
        if (report.hasError()) {
            showValidationReportDialog();
            QMessageBox::warning(this, QStringLiteral("生成阻止"), QStringLiteral("存在错误，请修复后再生成"));
            return;
        }
    }
    CppGenerator gen;
    if (gen.generate(rootParam, out)) {
        lastOutputDir = out;
        QMessageBox::information(this, QStringLiteral("生成代码"), QStringLiteral("生成成功"));
    } else {
        QMessageBox::warning(this, QStringLiteral("生成代码"), QStringLiteral("生成失败"));
    }
}

void MainWidget::onGenerateInstancesClicked()
{
    QStringList names;
    for (const auto& in : instances) names << in.name;
    InstancesSelectDialog sel(names, this);
    if (sel.exec() != QDialog::Accepted) return;
    const QStringList chosen = sel.selected();
    if (chosen.isEmpty()) return;
    QVector<InstanceMetadata> picked;
    for (const auto& in : instances) if (chosen.contains(in.name)) picked.push_back(in);
    const QString out = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"), lastOutputDir.isEmpty() ? QDir::homePath() : lastOutputDir);
    if (out.isEmpty()) return;
    CppGenerator gen;
    bool okCode = gen.generateInstances(rootParam, picked, out);
    bool okJson = gen.generateInstancesJson(rootParam, picked, out);
    if (okCode && okJson) {
        lastOutputDir = out;
        QMessageBox::information(this, QStringLiteral("生成实例代码"), QStringLiteral("生成成功（含 JSON 读写）"));
    } else {
        QMessageBox::warning(this, QStringLiteral("生成实例代码"), QStringLiteral("生成失败"));
    }
}
