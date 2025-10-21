#include "app/ui/ModEditorDialog.h"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QCompleter>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QDateTime>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSet>
#include <QSignalBlocker>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

namespace {

QString trimmed(const QString& text) {
  return text.trimmed();
}

QString joinTagDescriptor(const TagDescriptor& desc) {
  return QString::fromStdString(desc.group) + u":" + QString::fromStdString(desc.tag);
}

QString normalizeName(const QString& text) {
  QString normalized;
  normalized.reserve(text.size());
  for (const QChar& ch : text) {
    if (ch.isLetterOrNumber()) {
      normalized.append(ch.toLower());
    }
  }
  return normalized;
}

void ensureComboSelection(QComboBox* combo, const QString& value) {
  if (!combo) {
    return;
  }
  if (value.trimmed().isEmpty()) {
    if (combo->count() > 0) {
      combo->setCurrentIndex(0);
    }
    return;
  }
  const int existingIndex = combo->findData(value);
  if (existingIndex >= 0) {
    combo->setCurrentIndex(existingIndex);
    return;
  }
  combo->addItem(value, value);
  combo->setCurrentIndex(combo->count() - 1);
}

QString comboValue(const QComboBox* combo) {
  if (!combo) {
    return {};
  }
  const QVariant data = combo->currentData();
  if (data.isValid()) {
    return data.toString().trimmed();
  }
  return combo->currentText().trimmed();
}

} // namespace

ModEditorDialog::ModEditorDialog(RepositoryService& service, QWidget* parent)
    : QDialog(parent), service_(service) {
  setWindowTitle(tr("导入 / 编辑 MOD"));
  resize(620, 720);
  buildUi();
  loadAttributeOptions();
  loadCategories();
  loadTags();
}

void ModEditorDialog::buildUi() {
  auto* layout = new QVBoxLayout(this);

  auto* form = new QFormLayout();
  nameEdit_ = new QLineEdit(this);
  authorEdit_ = new QLineEdit(this);

  auto* categoryRow = new QHBoxLayout();
  primaryCategoryCombo_ = new QComboBox(this);
  setupSearchableCombo(primaryCategoryCombo_, tr("一级分类"));
  secondaryCategoryCombo_ = new QComboBox(this);
  setupSearchableCombo(secondaryCategoryCombo_, tr("二级分类"));
  addCategoryBtn_ = new QPushButton(tr("新建"), this);
  addCategoryBtn_->setToolTip(tr("创建顶级分类"));
  categoryRow->addWidget(primaryCategoryCombo_, 1);
  categoryRow->addWidget(secondaryCategoryCombo_, 1);
  categoryRow->addWidget(addCategoryBtn_);
  auto* categoryWrapper = new QWidget(this);
  categoryWrapper->setLayout(categoryRow);

  ratingSpin_ = new QSpinBox(this);
  ratingSpin_->setRange(0, 5);
  ratingSpin_->setSingleStep(1);
  ratingSpin_->setSpecialValueText(tr("未评分"));

  sizeSpin_ = new QDoubleSpinBox(this);
  sizeSpin_->setRange(0.0, 8192.0);
  sizeSpin_->setDecimals(2);
  sizeSpin_->setSuffix(" MB");
  sizeSpin_->setReadOnly(true);
  sizeSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);

  lastPublishedEdit_ = new QLineEdit(this);
  lastPublishedEdit_->setPlaceholderText("YYYY-MM-DD");
  lastSavedEdit_ = new QLineEdit(this);
  lastSavedEdit_->setPlaceholderText("YYYY-MM-DD");

  statusCombo_ = new QComboBox(this);
  statusCombo_->addItem(QStringLiteral("最新"), QStringLiteral("最新"));
  statusCombo_->addItem(QStringLiteral("过时"), QStringLiteral("过时"));
  statusCombo_->addItem(QStringLiteral("待检查"), QStringLiteral("待检查"));

  integrityCombo_ = new QComboBox(this);
  integrityCombo_->addItem(tr("-"), QString());
  stabilityCombo_ = new QComboBox(this);
  stabilityCombo_->addItem(tr("-"), QString());
  acquisitionCombo_ = new QComboBox(this);
  acquisitionCombo_->addItem(tr("-"), QString());

  sourcePlatformEdit_ = new QLineEdit(this);
  sourcePlatformEdit_->setPlaceholderText(tr("平台"));
  sourceUrlEdit_ = new QLineEdit(this);
  sourceUrlEdit_->setPlaceholderText("https://...");

  filePathEdit_ = new QLineEdit(this);
  browseFileBtn_ = new QPushButton(tr("浏览..."), this);

  coverPathEdit_ = new QLineEdit(this);
  browseCoverBtn_ = new QPushButton(tr("浏览..."), this);

  hashEdit_ = new QLineEdit(this);
  hashEdit_->setReadOnly(true);

  noteEdit_ = new QPlainTextEdit(this);
  noteEdit_->setPlaceholderText(tr("备注 / 说明..."));

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

  form->addRow(tr("名称*"), nameEdit_);
  form->addRow(tr("作者"), authorEdit_);
  form->addRow(tr("分类"), categoryWrapper);
  form->addRow(tr("评分"), ratingSpin_);
  form->addRow(tr("大小"), sizeSpin_);
  form->addRow(tr("最后发布日"), lastPublishedEdit_);
  form->addRow(tr("最后保存日"), lastSavedEdit_);
  form->addRow(tr("状态"), statusCombo_);
  form->addRow(tr("健全度"), integrityCombo_);
  form->addRow(tr("稳定性"), stabilityCombo_);
  form->addRow(tr("获取方式"), acquisitionCombo_);
  form->addRow(tr("平台"), sourcePlatformEdit_);
  form->addRow("URL", sourceUrlEdit_);
  form->addRow(tr("文件路径"), fileWrapper);
  form->addRow(tr("封面"), coverWrapper);
  form->addRow(tr("文件校验"), hashEdit_);
  layout->addLayout(form);

  auto* tagHeader = new QHBoxLayout();
  auto* tagLabel = new QLabel(tr("标签"), this);
  manageTagsButton_ = new QPushButton(tr("新建"), this);
  manageTagsButton_->setToolTip(tr("创建临时标签"));
  tagHeader->addWidget(tagLabel);
  tagHeader->addStretch();
  tagHeader->addWidget(manageTagsButton_);
  layout->addLayout(tagHeader);

  tagRowsContainer_ = new QWidget(this);
  tagRowsLayout_ = new QVBoxLayout(tagRowsContainer_);
  tagRowsLayout_->setContentsMargins(0, 0, 0, 0);
  tagRowsLayout_->setSpacing(6);
  layout->addWidget(tagRowsContainer_);

  buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  layout->addWidget(buttonBox_);

  connect(buttonBox_, &QDialogButtonBox::accepted, this, &ModEditorDialog::accept);
  connect(buttonBox_, &QDialogButtonBox::rejected, this, &ModEditorDialog::reject);
  connect(browseFileBtn_, &QPushButton::clicked, this, &ModEditorDialog::onBrowseFile);
  connect(browseCoverBtn_, &QPushButton::clicked, this, &ModEditorDialog::onBrowseCover);
  connect(addCategoryBtn_, &QPushButton::clicked, this, &ModEditorDialog::onAddCategory);
  connect(primaryCategoryCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ModEditorDialog::onPrimaryCategoryChanged);
  connect(filePathEdit_, &QLineEdit::textChanged, this, &ModEditorDialog::onFilePathEdited);
  connect(sourceUrlEdit_, &QLineEdit::textChanged, this, &ModEditorDialog::onSourceUrlEdited);
  connect(sourcePlatformEdit_, &QLineEdit::textEdited, this, &ModEditorDialog::onSourcePlatformEdited);
  connect(manageTagsButton_, &QPushButton::clicked, this, &ModEditorDialog::onAddTag);

  addTagRow();
}

void ModEditorDialog::setupSearchableCombo(QComboBox* combo, const QString& placeholder) {
  if (!combo) {
    return;
  }

  combo->setEditable(true);
  combo->setInsertPolicy(QComboBox::NoInsert);
  combo->setDuplicatesEnabled(false);
  combo->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);

  if (auto* line = combo->lineEdit()) {
    line->setPlaceholderText(placeholder);
    line->setClearButtonEnabled(true);
  }

  if (auto* completer = combo->completer()) {
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
  }
}

void ModEditorDialog::loadCategories() {
  categories_ = service_.listCategories();
  primaryCategories_.clear();
  secondaryCategories_.clear();

  for (const auto& cat : categories_) {
    if (cat.parent_id.has_value()) {
      secondaryCategories_[*cat.parent_id].push_back(cat);
    } else {
      primaryCategories_.push_back(cat);
    }
  }

  QSignalBlocker blocker1(primaryCategoryCombo_);
  QSignalBlocker blocker2(secondaryCategoryCombo_);

  primaryCategoryCombo_->clear();
  primaryCategoryCombo_->addItem(tr("未分类"), 0);
  for (const auto& cat : primaryCategories_) {
    primaryCategoryCombo_->addItem(QString::fromStdString(cat.name), cat.id);
  }

  rebuildSecondaryCategories(std::nullopt);
}

void ModEditorDialog::rebuildSecondaryCategories(std::optional<int> parentId) {
  QSignalBlocker blocker(secondaryCategoryCombo_);
  secondaryCategoryCombo_->clear();
  secondaryCategoryCombo_->addItem(tr("未分类"), 0);

  bool hasChildren = false;
  if (parentId) {
    const auto mapIt = secondaryCategories_.find(*parentId);
    if (mapIt != secondaryCategories_.end()) {
      const auto& children = mapIt->second;
      hasChildren = !children.empty();
      for (const auto& child : children) {
        secondaryCategoryCombo_->addItem(QString::fromStdString(child.name), child.id);
      }
    }
  }
  secondaryCategoryCombo_->setEnabled(hasChildren);
  secondaryCategoryCombo_->setCurrentIndex(0);
}

void ModEditorDialog::onPrimaryCategoryChanged(int index) {
  Q_UNUSED(index);
  const int parentId = primaryCategoryCombo_->currentData().toInt();
  if (parentId > 0) {
    rebuildSecondaryCategories(parentId);
  } else {
    rebuildSecondaryCategories(std::nullopt);
  }
}

void ModEditorDialog::loadTags() {
  tagGroups_ = service_.listTagGroups();
  tagItemsByGroup_.clear();

  for (const auto& group : tagGroups_) {
    const QString groupName = QString::fromStdString(group.name);
    if (!tagItemsByGroup_.contains(groupName)) {
      tagItemsByGroup_.insert(groupName, QStringList());
    }
  }

  auto allTags = service_.listTags();
  for (const auto& tag : allTags) {
    const QString groupName = QString::fromStdString(tag.group_name);
    const QString tagName = QString::fromStdString(tag.name);
    QStringList& list = tagItemsByGroup_[groupName];
    if (!list.contains(tagName)) {
      list.append(tagName);
    }
  }

  for (auto it = tagItemsByGroup_.begin(); it != tagItemsByGroup_.end(); ++it) {
    std::sort(it->begin(), it->end(), [](const QString& a, const QString& b) {
      return a.localeAwareCompare(b) < 0;
    });
  }

  if (tagRows_.empty()) {
    addTagRow();
  } else {
    for (auto& row : tagRows_) {
      refreshTagChoices(row.get());
    }
  }
}

void ModEditorDialog::loadAttributeOptions() {
  attributeOptions_ = config::loadModAttributeOptions();

  auto populateCombo = [&](QComboBox* combo, const std::vector<std::string>& values) {
    if (!combo) {
      return;
    }
    const QString previousValue = comboValue(combo);
    QSignalBlocker blocker(combo);
    for (int i = combo->count() - 1; i >= 1; --i) {
      combo->removeItem(i);
    }
    QSet<QString> seen;
    for (const auto& value : values) {
      const QString option = QString::fromStdString(value).trimmed();
      if (option.isEmpty()) {
        continue;
      }
      if (seen.contains(option)) {
        continue;
      }
      seen.insert(option);
      combo->addItem(option, option);
    }
    ensureComboSelection(combo, previousValue);
  };

  populateCombo(integrityCombo_, attributeOptions_.integrity);
  populateCombo(stabilityCombo_, attributeOptions_.stability);
  populateCombo(acquisitionCombo_, attributeOptions_.acquisition);
}

void ModEditorDialog::setMod(const ModRow& mod, const std::vector<TagDescriptor>& tags) {
  modId_ = mod.id;
  nameEdit_->setText(QString::fromStdString(mod.name));
  authorEdit_->setText(QString::fromStdString(mod.author));
  ratingSpin_->setValue(mod.rating);
  sizeSpin_->setValue(mod.size_mb);
  lastPublishedEdit_->setText(QString::fromStdString(mod.last_published_at));
  lastSavedEdit_->setText(QString::fromStdString(mod.last_saved_at));
  ensureComboSelection(statusCombo_, QString::fromStdString(mod.status));
  ensureComboSelection(integrityCombo_, QString::fromStdString(mod.integrity));
  ensureComboSelection(stabilityCombo_, QString::fromStdString(mod.stability));
  ensureComboSelection(acquisitionCombo_, QString::fromStdString(mod.acquisition_method));
  sourcePlatformEdit_->setText(QString::fromStdString(mod.source_platform));
  sourceUrlEdit_->setText(QString::fromStdString(mod.source_url));
  noteEdit_->setPlainText(QString::fromStdString(mod.note));
  hashEdit_->setText(QString::fromStdString(mod.file_hash));

  int primarySelection = 0;
  int secondarySelection = 0;

  if (mod.category_id > 0) {
    auto it = std::find_if(categories_.begin(), categories_.end(),
                           [&](const CategoryRow& row) { return row.id == mod.category_id; });
    if (it != categories_.end()) {
      if (it->parent_id.has_value()) {
        secondarySelection = it->id;
        primarySelection = *it->parent_id;
      } else {
        primarySelection = it->id;
      }
    }
  }

  {
    QSignalBlocker blocker(primaryCategoryCombo_);
    primaryCategoryCombo_->setCurrentIndex(
        primaryCategoryCombo_->findData(primarySelection));
  }
  onPrimaryCategoryChanged(primaryCategoryCombo_->currentIndex());
  secondaryCategoryCombo_->setCurrentIndex(
      secondaryCategoryCombo_->findData(secondarySelection));

  filePathEdit_->setText(QString::fromStdString(mod.file_path));
  coverPathEdit_->setText(QString::fromStdString(mod.cover_path));

  setCheckedTags(tags);
}

void ModEditorDialog::setCheckedTags(const std::vector<TagDescriptor>& tags) {
  clearTagRows();
  if (tags.empty()) {
    addTagRow();
    return;
  }
  for (const auto& desc : tags) {
    addTagRow(QString::fromStdString(desc.group), QString::fromStdString(desc.tag));
  }
}

ModRow ModEditorDialog::modData() const {
  ModRow mod;
  mod.id = modId_;
  mod.name = trimmed(nameEdit_->text()).toStdString();
  mod.author = trimmed(authorEdit_->text()).toStdString();
  mod.rating = ratingSpin_->value();

  const int primaryId = primaryCategoryCombo_->currentData().toInt();
  const int secondaryId = secondaryCategoryCombo_->currentData().toInt();
  if (secondaryId > 0) {
    mod.category_id = secondaryId;
  } else if (primaryId > 0) {
    mod.category_id = primaryId;
  } else {
    mod.category_id = 0;
  }

  mod.note = noteEdit_->toPlainText().trimmed().toStdString();
  mod.last_published_at = trimmed(lastPublishedEdit_->text()).toStdString();
  mod.last_saved_at = trimmed(lastSavedEdit_->text()).toStdString();
  QString statusValue = comboValue(statusCombo_);
  if (statusValue.isEmpty()) {
    statusValue = QStringLiteral("最新");
  }
  mod.status = statusValue.toStdString();
  mod.integrity = comboValue(integrityCombo_).toStdString();
  mod.stability = comboValue(stabilityCombo_).toStdString();
  mod.acquisition_method = comboValue(acquisitionCombo_).toStdString();
  mod.source_platform = trimmed(sourcePlatformEdit_->text()).toStdString();
  mod.source_url = trimmed(sourceUrlEdit_->text()).toStdString();
  mod.cover_path = trimmed(coverPathEdit_->text()).toStdString();
  mod.file_path = trimmed(filePathEdit_->text()).toStdString();
  mod.file_hash = trimmed(hashEdit_->text()).toStdString();
  mod.size_mb = sizeSpin_->value();
  return mod;
}

std::vector<TagDescriptor> ModEditorDialog::selectedTags() const {
  std::vector<TagDescriptor> tags;
  for (const auto& row : tagRows_) {
    const QString groupName = trimmed(row->groupCombo->currentText());
    const QString tagName = trimmed(row->tagCombo->currentText());
    if (groupName.isEmpty() || tagName.isEmpty()) {
      continue;
    }
    tags.push_back({groupName.toStdString(), tagName.toStdString()});
  }
  return tags;
}

void ModEditorDialog::accept() {
  if (trimmed(nameEdit_->text()).isEmpty()) {
    QMessageBox::warning(this, tr("缺少名称"), tr("请输入 MOD 名称。"));
    return;
  }
  if (trimmed(filePathEdit_->text()).isEmpty()) {
    QMessageBox::warning(this, tr("缺少文件"), tr("请选择 MOD 文件。"));
    return;
  }
  QDialog::accept();
}

void ModEditorDialog::onBrowseFile() {
  const QString path = QFileDialog::getOpenFileName(this, tr("选择文件"));
  if (!path.isEmpty()) {
    filePathEdit_->setText(path);
    applyFileMetadata(path);
  }
}

void ModEditorDialog::onBrowseCover() {
  const QString path = QFileDialog::getOpenFileName(
      this, tr("选择封面图片"), QString(), "Images (*.png *.jpg *.jpeg *.bmp *.webp);;All Files (*)");
  if (!path.isEmpty()) {
    coverPathEdit_->setText(path);
  }
}

void ModEditorDialog::onAddCategory() {
  bool ok = false;
  QString name =
      QInputDialog::getText(this, tr("新分类"), tr("分类名称："), QLineEdit::Normal, QString(), &ok).trimmed();
  if (!ok || name.isEmpty()) {
    return;
  }
  service_.createCategory(name.toStdString(), std::nullopt);
  loadCategories();
  primaryCategoryCombo_->setCurrentIndex(primaryCategoryCombo_->findText(name));
  onPrimaryCategoryChanged(primaryCategoryCombo_->currentIndex());
}

void ModEditorDialog::onAddTag() {
  bool ok = false;
  QString groupName =
      QInputDialog::getText(this, tr("标签组"), tr("组名称："), QLineEdit::Normal, QString(), &ok).trimmed();
  if (!ok || groupName.isEmpty()) {
    return;
  }
  QString tagName =
      QInputDialog::getText(this, tr("标签名称"), tr("标签："), QLineEdit::Normal, QString(), &ok).trimmed();
  if (!ok || tagName.isEmpty()) {
    return;
  }

  if (!tagItemsByGroup_.contains(groupName)) {
    tagItemsByGroup_.insert(groupName, QStringList());
    tagGroups_.push_back(TagGroupRow{0, groupName.toStdString(), 0});
  }
  QStringList& tags = tagItemsByGroup_[groupName];
  if (!tags.contains(tagName)) {
    tags.append(tagName);
    std::sort(tags.begin(), tags.end(), [](const QString& a, const QString& b) {
      return a.localeAwareCompare(b) < 0;
    });
  }

  for (auto& row : tagRows_) {
    refreshTagChoices(row.get());
  }
}

ModEditorDialog::TagRowWidgets* ModEditorDialog::addTagRow(const QString& group, const QString& tag, int insertIndex) {
  auto row = std::make_unique<TagRowWidgets>();
  row->container = new QWidget(tagRowsContainer_);
  auto* layout = new QHBoxLayout(row->container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  row->groupCombo = new QComboBox(row->container);
  row->tagCombo = new QComboBox(row->container);
  setupSearchableCombo(row->groupCombo, tr("标签组"));
  setupSearchableCombo(row->tagCombo, tr("标签"));

  row->addBtn = new QPushButton(QStringLiteral("+"), row->container);
  row->removeBtn = new QPushButton(QStringLiteral("-"), row->container);
  row->addBtn->setFixedWidth(32);
  row->removeBtn->setFixedWidth(32);
  row->addBtn->setToolTip(tr("添加标签行"));
  row->removeBtn->setToolTip(tr("移除该标签行"));

  layout->addWidget(row->groupCombo, 1);
  layout->addWidget(row->tagCombo, 1);
  layout->addWidget(row->addBtn);
  layout->addWidget(row->removeBtn);

  auto* rowPtr = row.get();

  connect(rowPtr->addBtn, &QPushButton::clicked, this, &ModEditorDialog::onAddTagRowClicked);
  connect(rowPtr->removeBtn, &QPushButton::clicked, this, [this, rowPtr]() { removeTagRow(rowPtr); });
  connect(rowPtr->groupCombo, &QComboBox::currentTextChanged, this, [this, rowPtr](const QString&) {
    refreshTagChoices(rowPtr);
  });

  if (insertIndex < 0 || insertIndex >= static_cast<int>(tagRows_.size())) {
    tagRowsLayout_->addWidget(rowPtr->container);
    tagRows_.push_back(std::move(row));
  } else {
    tagRowsLayout_->insertWidget(insertIndex, rowPtr->container);
    tagRows_.insert(tagRows_.begin() + insertIndex, std::move(row));
  }

  refreshTagChoices(rowPtr);
  if (!group.isEmpty()) {
    rowPtr->groupCombo->setCurrentText(group);
  }
  refreshTagChoices(rowPtr);
  if (!tag.isEmpty()) {
    rowPtr->tagCombo->setCurrentText(tag);
  }

  updateTagRowRemoveButtons();
  return rowPtr;
}

void ModEditorDialog::removeTagRow(ModEditorDialog::TagRowWidgets* row) {
  if (!row) return;

  if (tagRows_.size() <= 1) {
    if (row->groupCombo) {
      row->groupCombo->setCurrentIndex(-1);
      row->groupCombo->setEditText(QString());
    }
    if (row->tagCombo) {
      row->tagCombo->setCurrentIndex(-1);
      row->tagCombo->setEditText(QString());
    }
    refreshTagChoices(row);
    updateTagRowRemoveButtons();
    return;
  }

  auto it = std::find_if(tagRows_.begin(), tagRows_.end(),
                         [row](const std::unique_ptr<TagRowWidgets>& ptr) { return ptr.get() == row; });
  if (it != tagRows_.end()) {
    if (row->container) {
      tagRowsLayout_->removeWidget(row->container);
      row->container->deleteLater();
    }
    tagRows_.erase(it);
  }

  updateTagRowRemoveButtons();

  if (tagRows_.empty()) {
    addTagRow();
    updateTagRowRemoveButtons();
  }
}

void ModEditorDialog::onAddTagRowClicked() {
  auto* button = qobject_cast<QPushButton*>(sender());
  int insertIndex = -1;
  QString groupPrefill;

  if (button) {
    for (int i = 0; i < static_cast<int>(tagRows_.size()); ++i) {
      if (tagRows_[i] && tagRows_[i]->addBtn == button) {
        insertIndex = i + 1;
        if (tagRows_[i]->groupCombo) {
          groupPrefill = trimmed(tagRows_[i]->groupCombo->currentText());
        }
        break;
      }
    }
  }

  auto* row = addTagRow(groupPrefill, QString(), insertIndex);
  if (row && row->tagCombo) {
    row->tagCombo->setFocus();
  }
}

void ModEditorDialog::refreshTagChoices(ModEditorDialog::TagRowWidgets* row) {
  if (!row) return;

  QString groupValue = trimmed(row->groupCombo->currentText());
  QString tagValue = trimmed(row->tagCombo->currentText());

  {
    QSignalBlocker blocker(row->groupCombo);
    row->groupCombo->clear();
    row->groupCombo->addItem(QString());
    for (const auto& group : tagGroups_) {
      const QString name = QString::fromStdString(group.name);
      row->groupCombo->addItem(name);
    }
    for (auto it = tagItemsByGroup_.cbegin(); it != tagItemsByGroup_.cend(); ++it) {
      const QString name = it.key();
      if (row->groupCombo->findText(name) < 0) {
        row->groupCombo->addItem(name);
      }
    }
    if (!groupValue.isEmpty()) {
      row->groupCombo->setCurrentText(groupValue);
    }
  }

  groupValue = trimmed(row->groupCombo->currentText());
  {
    QSignalBlocker blocker(row->tagCombo);
    row->tagCombo->clear();
    row->tagCombo->addItem(QString());
    if (tagItemsByGroup_.contains(groupValue)) {
      const QStringList& tags = tagItemsByGroup_.value(groupValue);
      for (const QString& tag : tags) {
        row->tagCombo->addItem(tag);
      }
    }
    if (!tagValue.isEmpty()) {
      row->tagCombo->setCurrentText(tagValue);
    }
  }
}

void ModEditorDialog::clearTagRows() {
  for (auto& row : tagRows_) {
    if (row->container) {
      tagRowsLayout_->removeWidget(row->container);
      row->container->deleteLater();
    }
  }
  tagRows_.clear();
}

void ModEditorDialog::updateTagRowRemoveButtons() {
  const bool canRemove = tagRows_.size() > 1;
  for (auto& row : tagRows_) {
    if (row->removeBtn) {
      row->removeBtn->setEnabled(canRemove);
    }
  }
}

void ModEditorDialog::onFilePathEdited(const QString& path) {
  if (suppressFileSignal_) {
    return;
  }
  applyFileMetadata(path);
}

void ModEditorDialog::onSourceUrlEdited(const QString& url) {
  maybeAutoFillPlatform(url);
}

void ModEditorDialog::onSourcePlatformEdited(const QString& text) {
  const QString value = trimmed(text);
  if (value.isEmpty()) {
    platformEditedManually_ = false;
    lastAutoPlatform_.clear();
  } else {
    platformEditedManually_ = true;
    lastAutoPlatform_.clear();
  }
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

  const QDateTime lastModified = info.lastModified();
  if (lastModified.isValid()) {
    const QString dateText = lastModified.date().toString(QStringLiteral("yyyy-MM-dd"));
    if (modId_ == 0 || trimmed(lastSavedEdit_->text()).isEmpty()) {
      lastSavedEdit_->setText(dateText);
    }
    if (modId_ == 0 || trimmed(lastPublishedEdit_->text()).isEmpty()) {
      lastPublishedEdit_->setText(dateText);
    }
  }

  const QString coverCandidate = locateCoverSibling(info);
  if (!coverCandidate.isEmpty()) {
    if (modId_ == 0 || coverPathEdit_->text().trimmed().isEmpty()) {
      coverPathEdit_->setText(coverCandidate);
    }
  }
}

QString ModEditorDialog::locateCoverSibling(const QFileInfo& fileInfo) const {
  static const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.webp"};

  const QDir dir = fileInfo.dir();
  const QString base = fileInfo.completeBaseName();
  const QString modName = trimmed(nameEdit_->text());

  const QString normalizedBase = normalizeName(base);
  const QString normalizedModName = normalizeName(modName);

  QFileInfoList images = dir.entryInfoList(filters, QDir::Files | QDir::Readable);

  for (const QFileInfo& image : images) {
    if (normalizeName(image.completeBaseName()) == normalizedBase && !normalizedBase.isEmpty()) {
      return image.absoluteFilePath();
    }
  }

  if (!normalizedModName.isEmpty()) {
    for (const QFileInfo& image : images) {
      if (normalizeName(image.completeBaseName()).contains(normalizedModName)) {
        return image.absoluteFilePath();
      }
    }
  }

  return {};
}

void ModEditorDialog::maybeAutoFillPlatform(const QString& url) {
  const QString trimmedUrl = trimmed(url);
  if (trimmedUrl.isEmpty()) {
    return;
  }

  QUrl parsed = QUrl::fromUserInput(trimmedUrl);
  const QString host = parsed.host().toLower();
  if (host.isEmpty()) {
    return;
  }

  const QString current = trimmed(sourcePlatformEdit_->text());
  if (!platformEditedManually_ || current.isEmpty() || current == lastAutoPlatform_) {
    QSignalBlocker blocker(sourcePlatformEdit_);
    sourcePlatformEdit_->setText(host);
    lastAutoPlatform_ = host;
    platformEditedManually_ = false;
  }
}
