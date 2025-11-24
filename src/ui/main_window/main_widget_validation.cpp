#include "ui/main_window/main_widget.h"

#include "core/validation.h"

#include <QLabel>
#include <QMessageBox>

void MainWidget::updateValidationStatus()
{
    auto report = validateProject(rootParam);
    int errors = 0;
    int warns = 0;
    for (const auto& i : report.issues) {
        if (i.level == ValidationIssue::Error) ++errors;
        else ++warns;
    }
    statusLabel->setText(QStringLiteral("校验状态: ") +
                         (errors == 0 ? QStringLiteral("通过") : QStringLiteral("错误 ") + QString::number(errors)) +
                         QStringLiteral("  警告: ") + QString::number(warns));
}

void MainWidget::showValidationReportDialog()
{
    auto report = validateProject(rootParam);
    QString text;
    for (const auto& i : report.issues) {
        text += (i.level == ValidationIssue::Error ? QStringLiteral("[错误] ") : QStringLiteral("[警告] ")) + i.path + " - " + i.message + "\n";
    }
    if (text.isEmpty()) text = QStringLiteral("无问题");
    QMessageBox::information(this, QStringLiteral("校验报告"), text);
}

void MainWidget::onShowValidationClicked()
{
    showValidationReportDialog();
}
