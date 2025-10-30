#pragma once

#include <QDialog>

class QCheckBox;
class QDialogButtonBox;
class QLineEdit;
class QPushButton;

/**
 * @brief 批量导入对话框：用于选择目标文件夹与递归选项。
 */
class ImportFolderDialog : public QDialog {
  Q_OBJECT
public:
  explicit ImportFolderDialog(QWidget* parent = nullptr);

  QString directory() const;
  bool includeSubdirectories() const;

  void setDirectory(const QString& path);
  void setIncludeSubdirectories(bool enabled);

protected:
  void accept() override;

private slots:
  void onBrowse();

private:
  void buildUi();

  QLineEdit* directoryEdit_{};          // 输入目标目录
  QPushButton* browseButton_{};         // 浏览按钮
  QCheckBox* includeChildrenCheckBox_{}; // 是否包含子目录
  QDialogButtonBox* buttonBox_{};       // 确认/取消按钮组
};

