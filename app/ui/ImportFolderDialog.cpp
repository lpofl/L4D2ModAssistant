#include "app/ui/ImportFolderDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ImportFolderDialog::ImportFolderDialog(QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(tr("导入文件夹"));
  setModal(true);
  // 构建输入控件布局
  buildUi();
}

QString ImportFolderDialog::directory() const {
  return directoryEdit_ ? directoryEdit_->text().trimmed() : QString();
}

bool ImportFolderDialog::includeSubdirectories() const {
  return includeChildrenCheckBox_ && includeChildrenCheckBox_->isChecked();
}

void ImportFolderDialog::setDirectory(const QString& path) {
  if (directoryEdit_) {
    directoryEdit_->setText(path);
  }
}

void ImportFolderDialog::setIncludeSubdirectories(bool enabled) {
  if (includeChildrenCheckBox_) {
    includeChildrenCheckBox_->setChecked(enabled);
  }
}

void ImportFolderDialog::accept() {
  const QString dirPath = directory();
  QDir dir(dirPath);
  // 校验路径是否合法
  if (dirPath.isEmpty() || !dir.exists()) {
    QMessageBox::warning(this, tr("路径无效"), tr("请选择一个存在的文件夹路径"));
    return;
  }
  QDialog::accept();
}

void ImportFolderDialog::onBrowse() {
  const QString currentDir = directory();
  const QString path = QFileDialog::getExistingDirectory(this, tr("选择文件夹"), currentDir);
  if (!path.isEmpty() && directoryEdit_) {
    directoryEdit_->setText(path);
  }
}

void ImportFolderDialog::buildUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  // 文件夹路径输入行
  auto* pathRow = new QHBoxLayout();
  pathRow->setSpacing(8);

  auto* pathLabel = new QLabel(tr("文件夹路径"), this);
  directoryEdit_ = new QLineEdit(this);
  directoryEdit_->setPlaceholderText(tr("请选择包含 MOD 的文件夹"));
  browseButton_ = new QPushButton(tr("浏览..."), this);
  connect(browseButton_, &QPushButton::clicked, this, &ImportFolderDialog::onBrowse);

  pathRow->addWidget(pathLabel);
  pathRow->addWidget(directoryEdit_, 1);
  pathRow->addWidget(browseButton_);

  // 递归导入选项
  includeChildrenCheckBox_ = new QCheckBox(tr("包含子文件夹"), this);
  includeChildrenCheckBox_->setChecked(true);

  // 确认与取消按钮
  buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox_, &QDialogButtonBox::accepted, this, &ImportFolderDialog::accept);
  connect(buttonBox_, &QDialogButtonBox::rejected, this, &ImportFolderDialog::reject);

  layout->addLayout(pathRow);
  layout->addWidget(includeChildrenCheckBox_);
  layout->addWidget(buttonBox_);
  setLayout(layout);
}
