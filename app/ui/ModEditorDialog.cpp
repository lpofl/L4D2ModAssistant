#include "app/ui/ModEditorDialog.h"

#include <QComboBox>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

inline QString zh(const char* utf8) {
  return QString::fromUtf8(utf8);
}

QString joinTagDescriptor(const TagDescriptor& desc) {
  return QString::fromStdString(desc.group) + u":" + QString::fromStdString(desc.tag);
}

QString trimmedCopy(const QString& text) {
  return text.trimmed();
}

} // namespace

ModEditorDialog::ModEditorDialog(RepositoryService& service, QWidget* parent)
    : QDialog(parent), service_(service) {
  setWindowTitle(zh("导入 / 编辑 MOD"));
  resize(540, 680);
  buildUi();
  loadCategories();
  loadTags();
}

void ModEditorDialog::buildUi() {
  auto* layout = new QVBoxLayout(this);

  auto* form = new QFormLayout();
  nameEdit_ = new QLineEdit(this);
  authorEdit_ = new QLineEdit(this);

  auto* categoryRow = new QHBoxLayout();
  categoryCombo_ = new QComboBox(this);
  categoryCombo_->setEditable(false);
  addCategoryBtn_ = new QPushButton(zh("新建"), this);
  addCategoryBtn_->setToolTip(zh("新建分类"));
  categoryRow->addWidget(categoryCombo_, 1);
  categoryRow->addWidget(addCategoryBtn_);
  auto* categoryWrapper = new QWidget(this);
  categoryWrapper->setLayout(categoryRow);

  ratingSpin_ = new QSpinBox(this);
  ratingSpin_->setRange(0, 5);
  ratingSpin_->setSingleStep(1);
  ratingSpin_->setSpecialValueText(zh("未评分"));

  sizeSpin_ = new QDoubleSpinBox(this);
  sizeSpin_->setRange(0.0, 8192.0);
  sizeSpin_->setDecimals(2);
  sizeSpin_->setSuffix(" MB");
  sizeSpin_->setReadOnly(true);
  sizeSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);

  publishedEdit_ = new QLineEdit(this);
  publishedEdit_->setPlaceholderText("YYYY-MM-DD");

  sourceEdit_ = new QLineEdit(this);
  filePathEdit_ = new QLineEdit(this);
  browseFileBtn_ = new QPushButton(zh("浏览…"), this);

  coverPathEdit_ = new QLineEdit(this);
  browseCoverBtn_ = new QPushButton(zh("浏览…"), this);

  hashEdit_ = new QLineEdit(this);
  hashEdit_->setReadOnly(true);

  noteEdit_ = new QPlainTextEdit(this);
  noteEdit_->setPlaceholderText(zh("备注、使用说明等…"));

  auto* fileRow = new QHBoxLayout();
  fileRow->addWidget(filePathEdit_, 1);
  fileRow->addWidget(browseFileBtn_);
  auto* fileWrapper = new QWidget(this);
  fileWrapper->setLayout(fileRow);

  auto* coverRow = new QHBoxLayout();
  coverRow->addWidget(coverPathEdit_, 1);
  coverRow->addWidget(browseCoverBtn_);
  auto* coverWrapper = new QWidget(this);
  coverWrapper->setLayout(coverRow);

  form->addRow(zh("名称*"), nameEdit_);
  form->addRow(zh("作者"), authorEdit_);
  form->addRow(zh("分类"), categoryWrapper);
  form->addRow(zh("评分"), ratingSpin_);
  form->addRow(zh("体积"), sizeSpin_);
  form->addRow(zh("发布日期"), publishedEdit_);
  form->addRow(zh("来源"), sourceEdit_);
  form->addRow(zh("文件路径"), fileWrapper);
  form->addRow(zh("封面"), coverWrapper);
  form->addRow(zh("文件哈希"), hashEdit_);
  layout->addLayout(form);

  auto* tagLabel = new QLabel("TAG", this);
  layout->addWidget(tagLabel);

  tagTree_ = new QTreeWidget(this);
  tagTree_->setHeaderLabels({zh("标签组"), zh("标签")});
  tagTree_->setColumnCount(2);
  tagTree_->setRootIsDecorated(true);
  layout->addWidget(tagTree_, 1);

  auto* tagInputRow = new QHBoxLayout();
  tagGroupCombo_ = new QComboBox(this);
  tagGroupCombo_->setEditable(true);
  newTagEdit_ = new QLineEdit(this);
  newTagEdit_->setPlaceholderText(zh("标签名称"));
  addTagBtn_ = new QPushButton(zh("添加标签"), this);
  tagInputRow->addWidget(tagGroupCombo_, 1);
  tagInputRow->addWidget(newTagEdit_, 1);
  tagInputRow->addWidget(addTagBtn_);
  auto* tagInputWrapper = new QWidget(this);
  tagInputWrapper->setLayout(tagInputRow);
  layout->addWidget(tagInputWrapper);

  auto* noteLabel = new QLabel(zh("备注"), this);
  layout->addWidget(noteLabel);
  layout->addWidget(noteEdit_, 1);

  buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  layout->addWidget(buttonBox_);

  connect(buttonBox_, &QDialogButtonBox::accepted, this, &ModEditorDialog::accept);
  connect(buttonBox_, &QDialogButtonBox::rejected, this, &ModEditorDialog::reject);
  connect(browseFileBtn_, &QPushButton::clicked, this, &ModEditorDialog::onBrowseFile);
  connect(browseCoverBtn_, &QPushButton::clicked, this, &ModEditorDialog::onBrowseCover);
  connect(addCategoryBtn_, &QPushButton::clicked, this, &ModEditorDialog::onAddCategory);
  connect(addTagBtn_, &QPushButton::clicked, this, &ModEditorDialog::onAddTag);
  connect(filePathEdit_, &QLineEdit::textChanged, this, &ModEditorDialog::onFilePathEdited);
}

void ModEditorDialog::loadCategories() {
  categoryCombo_->clear();
  categoryCombo_->addItem(zh("未分类"), 0);
  categories_ = service_.listCategories();
  for (const auto& row : categories_) {
    categoryCombo_->addItem(QString::fromStdString(row.name), row.id);
  }
}

void ModEditorDialog::loadTags() {
  tagTree_->clear();
  tagGroupCombo_->clear();

  tagGroups_ = service_.listTagGroups();
  for (const auto& group : tagGroups_) {
    tagGroupCombo_->addItem(QString::fromStdString(group.name));
  }

  auto allTags = service_.listTags();
  QHash<QString, QTreeWidgetItem*> groupItems;
  for (const auto& tag : allTags) {
    const QString groupName = QString::fromStdString(tag.group_name);
    QTreeWidgetItem* parent = groupItems.value(groupName, nullptr);
    if (!parent) {
      parent = new QTreeWidgetItem(tagTree_);
      parent->setText(0, groupName);
      parent->setFirstColumnSpanned(true);
      parent->setFlags(parent->flags() | Qt::ItemIsUserCheckable);
      parent->setCheckState(0, Qt::Unchecked);
      groupItems.insert(groupName, parent);
    }

    auto* child = new QTreeWidgetItem(parent);
    child->setText(0, groupName);
    child->setText(1, QString::fromStdString(tag.name));
    child->setFlags(child->flags() | Qt::ItemIsUserCheckable);
    child->setCheckState(0, Qt::Unchecked);
    child->setCheckState(1, Qt::Unchecked);
  }
  tagTree_->expandAll();
}

void ModEditorDialog::setMod(const ModRow& mod, const std::vector<TagDescriptor>& tags) {
  modId_ = mod.id;
  nameEdit_->setText(QString::fromStdString(mod.name));
  authorEdit_->setText(QString::fromStdString(mod.author));
  ratingSpin_->setValue(mod.rating);
  sizeSpin_->setValue(mod.size_mb);
  publishedEdit_->setText(QString::fromStdString(mod.published_at));
  sourceEdit_->setText(QString::fromStdString(mod.source));
  noteEdit_->setPlainText(QString::fromStdString(mod.note));
  hashEdit_->setText(QString::fromStdString(mod.file_hash));

  int index = categoryCombo_->findData(mod.category_id);
  if (index >= 0) {
    categoryCombo_->setCurrentIndex(index);
  }

  suppressFileSignal_ = true;
  filePathEdit_->setText(QString::fromStdString(mod.file_path));
  coverPathEdit_->setText(QString::fromStdString(mod.cover_path));
  suppressFileSignal_ = false;

  setCheckedTags(tags);
}

void ModEditorDialog::setCheckedTags(const std::vector<TagDescriptor>& tags) {
  QSet<QString> needed;
  for (const auto& desc : tags) {
    needed.insert(joinTagDescriptor(desc));
  }

  const auto items = tagTree_->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
  for (QTreeWidgetItem* item : items) {
    if (item->childCount() == 0) {
      const QString descriptor = item->text(0) + u":" + item->text(1);
      if (needed.contains(descriptor)) {
        item->setCheckState(0, Qt::Checked);
        item->setCheckState(1, Qt::Checked);
      }
    }
  }
}

ModRow ModEditorDialog::modData() const {
  ModRow mod;
  mod.id = modId_;
  mod.name = trimmedCopy(nameEdit_->text()).toStdString();
  mod.author = trimmedCopy(authorEdit_->text()).toStdString();
  mod.rating = ratingSpin_->value();
  mod.category_id = categoryCombo_->currentData().toInt();
  mod.note = noteEdit_->toPlainText().trimmed().toStdString();
  mod.published_at = trimmedCopy(publishedEdit_->text()).toStdString();
  mod.source = trimmedCopy(sourceEdit_->text()).toStdString();
  mod.cover_path = trimmedCopy(coverPathEdit_->text()).toStdString();
  mod.file_path = trimmedCopy(filePathEdit_->text()).toStdString();
  mod.file_hash = trimmedCopy(hashEdit_->text()).toStdString();
  mod.size_mb = sizeSpin_->value();
  return mod;
}

std::vector<TagDescriptor> ModEditorDialog::selectedTags() const {
  std::vector<TagDescriptor> tags;
  const int topCount = tagTree_->topLevelItemCount();
  for (int i = 0; i < topCount; ++i) {
    QTreeWidgetItem* parent = tagTree_->topLevelItem(i);
    const int childCount = parent->childCount();
    for (int j = 0; j < childCount; ++j) {
      QTreeWidgetItem* child = parent->child(j);
      if (child->checkState(0) == Qt::Checked || child->checkState(1) == Qt::Checked) {
        tags.push_back({parent->text(0).toStdString(), child->text(1).toStdString()});
      }
    }
  }
  return tags;
}

void ModEditorDialog::accept() {
  if (trimmedCopy(nameEdit_->text()).isEmpty()) {
    QMessageBox::warning(this, zh("缺少名称"), zh("请填写 MOD 名称。"));
    return;
  }
  if (trimmedCopy(filePathEdit_->text()).isEmpty()) {
    QMessageBox::warning(this, zh("缺少文件"), zh("请选择 MOD 文件。"));
    return;
  }
  QDialog::accept();
}

void ModEditorDialog::onBrowseFile() {
  const QString path = QFileDialog::getOpenFileName(this, zh("选择文件"));
  if (!path.isEmpty()) {
    filePathEdit_->setText(path);
    applyFileMetadata(path);
  }
}

void ModEditorDialog::onBrowseCover() {
  const QString path = QFileDialog::getOpenFileName(
      this, zh("选择封面图片"), QString(), "Images (*.png *.jpg *.jpeg *.bmp *.webp);;All Files (*)");
  if (!path.isEmpty()) {
    coverPathEdit_->setText(path);
  }
}

void ModEditorDialog::onAddCategory() {
  bool ok = false;
  QString name =
      QInputDialog::getText(this, zh("新建分类"), zh("分类名称："), QLineEdit::Normal, QString(), &ok).trimmed();
  if (!ok || name.isEmpty()) {
    return;
  }
  service_.createCategory(name.toStdString(), std::nullopt);
  loadCategories();
  int index = categoryCombo_->findText(name);
  if (index >= 0) {
    categoryCombo_->setCurrentIndex(index);
  }
}

void ModEditorDialog::onAddTag() {
  const QString groupName = trimmedCopy(tagGroupCombo_->currentText());
  const QString tagName = trimmedCopy(newTagEdit_->text());
  if (groupName.isEmpty() || tagName.isEmpty()) {
    return;
  }
  if (tagGroupCombo_->findText(groupName) < 0) {
    tagGroupCombo_->addItem(groupName);
  }
  QTreeWidgetItem* groupItem = ensureGroupItem(groupName);
  const int childCount = groupItem->childCount();
  for (int i = 0; i < childCount; ++i) {
    QTreeWidgetItem* child = groupItem->child(i);
    if (child->text(1).compare(tagName, Qt::CaseInsensitive) == 0) {
      child->setCheckState(0, Qt::Checked);
      child->setCheckState(1, Qt::Checked);
      newTagEdit_->clear();
      return;
    }
  }

  auto* child = new QTreeWidgetItem(groupItem);
  child->setText(0, groupName);
  child->setText(1, tagName);
  child->setFlags(child->flags() | Qt::ItemIsUserCheckable);
  child->setCheckState(0, Qt::Checked);
  child->setCheckState(1, Qt::Checked);
  groupItem->setExpanded(true);
  newTagEdit_->clear();
}

QTreeWidgetItem* ModEditorDialog::ensureGroupItem(const QString& groupName) {
  const int topCount = tagTree_->topLevelItemCount();
  for (int i = 0; i < topCount; ++i) {
    QTreeWidgetItem* item = tagTree_->topLevelItem(i);
    if (item->text(0).compare(groupName, Qt::CaseInsensitive) == 0) {
      return item;
    }
  }
  auto* item = new QTreeWidgetItem(tagTree_);
  item->setText(0, groupName);
  item->setFirstColumnSpanned(true);
  item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
  item->setCheckState(0, Qt::Unchecked);
  return item;
}

void ModEditorDialog::onFilePathEdited(const QString& path) {
  if (suppressFileSignal_) {
    return;
  }
  applyFileMetadata(path);
}

void ModEditorDialog::applyFileMetadata(const QString& path) {
  QFileInfo info(path);
  if (!info.exists() || !info.isFile()) {
    return;
  }

  const QString baseName = info.completeBaseName();
  if (modId_ == 0 || nameEdit_->text().trimmed().isEmpty()) {
    nameEdit_->setText(baseName);
  }

  const double sizeMb = static_cast<double>(info.size()) / (1024.0 * 1024.0);
  sizeSpin_->setValue(sizeMb);

  QFile file(info.absoluteFilePath());
  if (file.open(QIODevice::ReadOnly)) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
      hash.addData(file.read(1 << 16));
    }
    hashEdit_->setText(QString::fromLatin1(hash.result().toHex()));
  }

  const QString coverCandidate = locateCoverSibling(info);
  if (!coverCandidate.isEmpty()) {
    if (modId_ == 0 || coverPathEdit_->text().trimmed().isEmpty()) {
      coverPathEdit_->setText(coverCandidate);
    }
  }
}

QString ModEditorDialog::locateCoverSibling(const QFileInfo& fileInfo) const {
  static const QStringList extensions = {".png", ".jpg", ".jpeg", ".bmp", ".webp"};
  const QDir dir = fileInfo.dir();
  const QString base = fileInfo.completeBaseName();
  for (const QString& ext : extensions) {
    const QString candidate = dir.absoluteFilePath(base + ext);
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
  }
  return {};
}
