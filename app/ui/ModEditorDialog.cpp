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

ModEditorDialog::RelationKind ModEditorDialog::relationKindFromData(int value) {
  switch (value) {
    case 0: return ModEditorDialog::RelationKind::Conflict;
    case 1: return ModEditorDialog::RelationKind::Requires;
    case 2: return ModEditorDialog::RelationKind::RequiredBy;
    case 3: return ModEditorDialog::RelationKind::Homologous;
    case 4: return ModEditorDialog::RelationKind::CustomMaster;
    case 5: return ModEditorDialog::RelationKind::CustomSlave;
    default: return ModEditorDialog::RelationKind::Conflict;
  }
}

ModEditorDialog::RelationTarget ModEditorDialog::relationTargetFromData(int value) {
  switch (value) {
    case 0: return ModEditorDialog::RelationTarget::Mod;
    case 1: return ModEditorDialog::RelationTarget::Category;
    case 2: return ModEditorDialog::RelationTarget::Tag;
    default: return ModEditorDialog::RelationTarget::Mod;
  }
}

int ModEditorDialog::toInt(ModEditorDialog::RelationKind kind) {
  switch (kind) {
    case ModEditorDialog::RelationKind::Conflict: return 0;
    case ModEditorDialog::RelationKind::Requires: return 1;
    case ModEditorDialog::RelationKind::RequiredBy: return 2;
    case ModEditorDialog::RelationKind::Homologous: return 3;
    case ModEditorDialog::RelationKind::CustomMaster: return 4;
    case ModEditorDialog::RelationKind::CustomSlave: return 5;
  }
  return 0;
}

int ModEditorDialog::toInt(ModEditorDialog::RelationTarget target) {
  switch (target) {
    case ModEditorDialog::RelationTarget::Mod: return 0;
    case ModEditorDialog::RelationTarget::Category: return 1;
    case ModEditorDialog::RelationTarget::Tag: return 2;
  }
  return 0;
}

ModEditorDialog::ModEditorDialog(RepositoryService& service, QWidget* parent)
    : QDialog(parent), service_(service) {
  setWindowTitle(tr("导入 / 编辑 MOD"));
  resize(620, 720);
  buildUi();
  loadAttributeOptions();
  loadCategories();
  loadTags();
  loadRelationSources();
}

void ModEditorDialog::buildUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  form->setSpacing(10);

  nameEdit_ = new QLineEdit(this);
  form->addRow(tr("名称*"), nameEdit_);

  auto* categoryRow = new QHBoxLayout();
  categoryRow->setContentsMargins(0, 0, 0, 0);
  categoryRow->setSpacing(8);
  primaryCategoryCombo_ = new QComboBox(this);
  setupSearchableCombo(primaryCategoryCombo_, tr("一级分类"));
  secondaryCategoryCombo_ = new QComboBox(this);
  setupSearchableCombo(secondaryCategoryCombo_, tr("二级分类"));
  addCategoryBtn_ = new QPushButton(tr("新建分类"), this);
  addCategoryBtn_->setToolTip(tr("创建顶级分类"));
  categoryRow->addWidget(primaryCategoryCombo_, 1);
  categoryRow->addWidget(secondaryCategoryCombo_, 1);
  categoryRow->addWidget(addCategoryBtn_);
  auto* categoryWrapper = new QWidget(this);
  categoryWrapper->setLayout(categoryRow);
  form->addRow(tr("分类"), categoryWrapper);

  tagRowsContainer_ = new QWidget(this);
  tagRowsLayout_ = new QVBoxLayout(tagRowsContainer_);
  tagRowsLayout_->setContentsMargins(0, 0, 0, 0);
  tagRowsLayout_->setSpacing(6);
  auto* tagWrapper = new QWidget(this);
  auto* tagWrapperLayout = new QVBoxLayout(tagWrapper);
  tagWrapperLayout->setContentsMargins(0, 0, 0, 0);
  auto* tagHeader = new QHBoxLayout();
  tagHeader->setContentsMargins(0, 0, 0, 0);
  manageTagsButton_ = new QPushButton(tr("新建标签"), tagWrapper);
  manageTagsButton_->setToolTip(tr("创建临时标签"));
  tagHeader->addWidget(manageTagsButton_);
  tagHeader->addStretch();
  addTagRowButton_ = new QPushButton(QStringLiteral("+"), tagWrapper);
  addTagRowButton_->setFixedWidth(28);
  addTagRowButton_->setToolTip(tr("添加标签行"));
  tagHeader->addWidget(addTagRowButton_);
  tagWrapperLayout->addLayout(tagHeader);
  tagWrapperLayout->addWidget(tagRowsContainer_);
  form->addRow(tr("标签"), tagWrapper);

  // ========== 作者 / 评分 ==========
auto* authorLabel = new QLabel(tr("作者"), this);
authorEdit_ = new QLineEdit(this);
authorEdit_->setPlaceholderText(tr("输入作者"));

auto* ratingLabel = new QLabel(tr("评分"), this);
ratingSpin_ = new QSpinBox(this);
ratingSpin_->setRange(0, 5);
ratingSpin_->setSingleStep(1);
ratingSpin_->setSpecialValueText(tr("未评分"));
ratingSpin_->setFixedWidth(80);

auto* authorRatingRow = new QWidget(this);
auto* authorRatingLayout = new QHBoxLayout(authorRatingRow);
authorRatingLayout->setContentsMargins(0, 0, 0, 0);
authorRatingLayout->setSpacing(8);

authorRatingLayout->addWidget(authorLabel);
authorRatingLayout->addWidget(authorEdit_, 1);
authorRatingLayout->addWidget(ratingLabel);
authorRatingLayout->addWidget(ratingSpin_);
authorRatingLayout->addStretch();

form->addRow(authorRatingRow);


// ========== 稳定性 / 大小 ==========
auto* stabilityLabel = new QLabel(tr("稳定性"), this);
stabilityCombo_ = new QComboBox(this);
stabilityCombo_->addItem(tr("-"), QString());

auto* sizeLabel = new QLabel(tr("大小"), this);
sizeSpin_ = new QDoubleSpinBox(this);
sizeSpin_->setRange(0.0, 8192.0);
sizeSpin_->setDecimals(2);
sizeSpin_->setSuffix(tr(" MB"));
sizeSpin_->setReadOnly(true);
sizeSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);

auto* stabilitySizeRow = new QWidget(this);
auto* stabilitySizeLayout = new QHBoxLayout(stabilitySizeRow);
stabilitySizeLayout->setContentsMargins(0, 0, 0, 0);
stabilitySizeLayout->setSpacing(8);

stabilitySizeLayout->addWidget(stabilityLabel);
stabilitySizeLayout->addWidget(stabilityCombo_, 1);
stabilitySizeLayout->addWidget(sizeLabel);
stabilitySizeLayout->addWidget(sizeSpin_);
stabilitySizeLayout->addStretch();

form->addRow(stabilitySizeRow);


// ========== 健全度 / 获取方式 ==========
auto* integrityLabel = new QLabel(tr("健全度"), this);
integrityCombo_ = new QComboBox(this);
integrityCombo_->addItem(tr("-"), QString());

auto* acquisitionLabel = new QLabel(tr("获取方式"), this);
acquisitionCombo_ = new QComboBox(this);
acquisitionCombo_->addItem(tr("-"), QString());

auto* integrityAcquisitionRow = new QWidget(this);
auto* integrityAcquisitionLayout = new QHBoxLayout(integrityAcquisitionRow);
integrityAcquisitionLayout->setContentsMargins(0, 0, 0, 0);
integrityAcquisitionLayout->setSpacing(8);

integrityAcquisitionLayout->addWidget(integrityLabel);
integrityAcquisitionLayout->addWidget(integrityCombo_, 1);
integrityAcquisitionLayout->addWidget(acquisitionLabel);
integrityAcquisitionLayout->addWidget(acquisitionCombo_, 1);
integrityAcquisitionLayout->addStretch();

form->addRow(integrityAcquisitionRow);


// ========== 发布平台 / URL ==========
auto* platformLabel = new QLabel(tr("发布平台"), this);
sourcePlatformEdit_ = new QLineEdit(this);
sourcePlatformEdit_->setPlaceholderText(tr("平台名"));
sourcePlatformEdit_->setMaximumWidth(160);

auto* urlLabel = new QLabel(tr("URL"), this);
sourceUrlEdit_ = new QLineEdit(this);
sourceUrlEdit_->setPlaceholderText("https://...");

auto* platformUrlRow = new QWidget(this);
auto* platformUrlLayout = new QHBoxLayout(platformUrlRow);
platformUrlLayout->setContentsMargins(0, 0, 0, 0);
platformUrlLayout->setSpacing(8);

platformUrlLayout->addWidget(platformLabel);
platformUrlLayout->addWidget(sourcePlatformEdit_);
platformUrlLayout->addWidget(urlLabel);
platformUrlLayout->addWidget(sourceUrlEdit_, 1);
platformUrlLayout->addStretch();

form->addRow(platformUrlRow);


// ========== 最后发布日 / 保存日 / 版本状态 ==========
auto* lastPublishedLabel = new QLabel(tr("最后发布日"), this);
lastPublishedEdit_ = new QLineEdit(this);
lastPublishedEdit_->setPlaceholderText("YYYY-MM-DD");

auto* lastSavedLabel = new QLabel(tr("最后保存日"), this);
lastSavedEdit_ = new QLineEdit(this);
lastSavedEdit_->setPlaceholderText("YYYY-MM-DD");

auto* statusLabel = new QLabel(tr("版本状态"), this);
statusCombo_ = new QComboBox(this);
statusCombo_->addItems({ tr("最新"), tr("过时"), tr("待检查") });
statusCombo_->setMinimumWidth(120);

auto* timelineRow = new QWidget(this);
auto* timelineLayout = new QHBoxLayout(timelineRow);
timelineLayout->setContentsMargins(0, 0, 0, 0);
timelineLayout->setSpacing(8);

timelineLayout->addWidget(lastPublishedLabel);
timelineLayout->addWidget(lastPublishedEdit_);
timelineLayout->addWidget(lastSavedLabel);
timelineLayout->addWidget(lastSavedEdit_);
timelineLayout->addWidget(statusLabel);
timelineLayout->addWidget(statusCombo_);
timelineLayout->addStretch();

form->addRow(timelineRow);

  relationRowsContainer_ = new QWidget(this);
  relationRowsLayout_ = new QVBoxLayout(relationRowsContainer_);
  relationRowsLayout_->setContentsMargins(0, 0, 0, 0);
  relationRowsLayout_->setSpacing(6);
  auto* relationWrapper = new QWidget(this);
  auto* relationWrapperLayout = new QVBoxLayout(relationWrapper);
  relationWrapperLayout->setContentsMargins(0, 0, 0, 0);
  relationWrapperLayout->addWidget(relationRowsContainer_);
  form->addRow(tr("MOD 关系"), relationWrapper);

  filePathEdit_ = new QLineEdit(this);
  browseFileBtn_ = new QPushButton(tr("浏览..."), this);
  auto* fileRow = new QHBoxLayout();
  fileRow->setContentsMargins(0, 0, 0, 0);
  fileRow->setSpacing(8);
  fileRow->addWidget(filePathEdit_, 1);
  fileRow->addWidget(browseFileBtn_);
  auto* fileWrapper = new QWidget(this);
  fileWrapper->setLayout(fileRow);
  form->addRow(tr("文件路径"), fileWrapper);

  coverPathEdit_ = new QLineEdit(this);
  browseCoverBtn_ = new QPushButton(tr("浏览..."), this);
  auto* coverRow = new QHBoxLayout();
  coverRow->setContentsMargins(0, 0, 0, 0);
  coverRow->setSpacing(8);
  coverRow->addWidget(coverPathEdit_, 1);
  coverRow->addWidget(browseCoverBtn_);
  auto* coverWrapper = new QWidget(this);
  coverWrapper->setLayout(coverRow);
  form->addRow(tr("封面路径"), coverWrapper);

  hashEdit_ = new QLineEdit(this);
  hashEdit_->setReadOnly(true);
  form->addRow(tr("文件校验"), hashEdit_);

  noteEdit_ = new QPlainTextEdit(this);
  noteEdit_->setPlaceholderText(tr("备注 / 说明..."));
  form->addRow(tr("备注"), noteEdit_);

  layout->addLayout(form);

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
  connect(addTagRowButton_, &QPushButton::clicked, this, &ModEditorDialog::onAddTagRowClicked);

  addTagRow();
  addRelationRow();
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
  loadRelationSources();
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
  loadRelationSources();
}

void ModEditorDialog::setupSearchableCombo(QComboBox* combo, const QString& placeholder) {
  if (!combo) {
    return;
  }
  combo->setEditable(true);
  combo->setInsertPolicy(QComboBox::NoInsert);
  QLineEdit* editor = combo->lineEdit();
  if (!editor) {
    editor = new QLineEdit(combo);
    combo->setLineEdit(editor);
  }
  editor->setPlaceholderText(placeholder);
  editor->setClearButtonEnabled(true);
}

ModEditorDialog::TagRowWidgets* ModEditorDialog::addTagRow(const QString& group,
                                                           const QString& tag,
                                                           int insertIndex) {
  auto row = std::make_unique<TagRowWidgets>();
  row->container = new QWidget(tagRowsContainer_);
  auto* layout = new QHBoxLayout(row->container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  row->groupCombo = new QComboBox(row->container);
  setupSearchableCombo(row->groupCombo, tr("标签组"));
  row->tagCombo = new QComboBox(row->container);
  setupSearchableCombo(row->tagCombo, tr("标签"));

  row->addBtn = new QPushButton(QStringLiteral("+"), row->container);
  row->removeBtn = new QPushButton(QStringLiteral("-"), row->container);
  row->addBtn->setFixedWidth(28);
  row->removeBtn->setFixedWidth(28);
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
    refreshTagChoices(rowPtr);
  }
  if (!tag.isEmpty()) {
    rowPtr->tagCombo->setCurrentText(tag);
  }

  updateTagRowRemoveButtons();
  return rowPtr;
}

void ModEditorDialog::removeTagRow(TagRowWidgets* row) {
  if (!row) {
    return;
  }

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
                         [&](const std::unique_ptr<TagRowWidgets>& holder) { return holder.get() == row; });
  if (it != tagRows_.end()) {
    if (row->container) {
      tagRowsLayout_->removeWidget(row->container);
      row->container->deleteLater();
    }
    tagRows_.erase(it);
  }

  if (tagRows_.empty()) {
    addTagRow();
  }

  updateTagRowRemoveButtons();
}

void ModEditorDialog::clearTagRows() {
  for (auto& row : tagRows_) {
    if (row && row->container) {
      tagRowsLayout_->removeWidget(row->container);
      row->container->deleteLater();
    }
  }
  tagRows_.clear();
}

void ModEditorDialog::updateTagRowRemoveButtons() {
  const bool canRemove = tagRows_.size() > 1;
  for (auto& row : tagRows_) {
    if (row && row->removeBtn) {
      row->removeBtn->setEnabled(canRemove);
    }
  }
}

void ModEditorDialog::refreshTagChoices(TagRowWidgets* row) {
  if (!row) {
    return;
  }

  const QString previousGroup = trimmed(row->groupCombo->currentText());
  const QString previousTag = trimmed(row->tagCombo->currentText());

  {
    QSignalBlocker blocker(row->groupCombo);
    row->groupCombo->clear();
    row->groupCombo->addItem(QString());
    for (const auto& group : tagGroups_) {
      row->groupCombo->addItem(QString::fromStdString(group.name));
    }
    for (auto it = tagItemsByGroup_.cbegin(); it != tagItemsByGroup_.cend(); ++it) {
      const QString& name = it.key();
      if (row->groupCombo->findText(name) < 0) {
        row->groupCombo->addItem(name);
      }
    }
    if (!previousGroup.isEmpty()) {
      row->groupCombo->setCurrentText(previousGroup);
    }
  }

  const QString groupValue = trimmed(row->groupCombo->currentText());
  {
    QSignalBlocker blocker(row->tagCombo);
    row->tagCombo->clear();
    row->tagCombo->addItem(QString());
    auto it = tagItemsByGroup_.find(groupValue);
    if (it != tagItemsByGroup_.end()) {
      for (const QString& item : it.value()) {
        row->tagCombo->addItem(item);
      }
    }
    if (!previousTag.isEmpty()) {
      row->tagCombo->setCurrentText(previousTag);
    }
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

void ModEditorDialog::loadRelationSources() {
  relationModOptions_ = service_.listVisible();
  if (modId_ > 0) {
    relationModOptions_.erase(std::remove_if(relationModOptions_.begin(), relationModOptions_.end(),
                                             [&](const ModRow& row) { return row.id == modId_; }),
                               relationModOptions_.end());
  }
  relationCategoryOptions_ = categories_;

  relationTagOptions_.clear();
  for (auto it = tagItemsByGroup_.cbegin(); it != tagItemsByGroup_.cend(); ++it) {
    const QString& groupName = it.key();
    const QStringList& tags = it.value();
    for (const QString& tag : tags) {
      relationTagOptions_.append(QStringLiteral("%1: %2").arg(groupName, tag));
    }
  }
  std::sort(relationTagOptions_.begin(), relationTagOptions_.end(), [](const QString& a, const QString& b) {
    return a.localeAwareCompare(b) < 0;
  });

  if (!relationRowsContainer_) {
    return;
  }

  for (auto& row : relationRows_) {
    updateRelationRowKind(row.get());
    refreshRelationRowChoices(row.get());
  }
}

ModEditorDialog::RelationRowWidgets* ModEditorDialog::addRelationRow(RelationKind kind,
                                                                     RelationTarget target,
                                                                     const QString& value,
                                                                     const QString& slot,
                                                                     int insertIndex) {
  auto row = std::make_unique<RelationRowWidgets>();
  row->container = new QWidget(relationRowsContainer_);
  auto* layout = new QHBoxLayout(row->container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  row->kindCombo = new QComboBox(row->container);
  row->kindCombo->addItem(tr("冲突"), toInt(RelationKind::Conflict));
  row->kindCombo->addItem(tr("前置"), toInt(RelationKind::Requires));
  row->kindCombo->addItem(tr("后置"), toInt(RelationKind::RequiredBy));
  row->kindCombo->addItem(tr("同质"), toInt(RelationKind::Homologous));
  row->kindCombo->addItem(tr("自定义（主）"), toInt(RelationKind::CustomMaster));
  row->kindCombo->addItem(tr("自定义（从）"), toInt(RelationKind::CustomSlave));
  row->kindCombo->setCurrentIndex(row->kindCombo->findData(toInt(kind)));

  row->targetTypeCombo = new QComboBox(row->container);
  row->targetTypeCombo->addItem(tr("MOD"), toInt(RelationTarget::Mod));
  row->targetTypeCombo->addItem(tr("分类"), toInt(RelationTarget::Category));
  row->targetTypeCombo->addItem(tr("标签"), toInt(RelationTarget::Tag));
  row->targetTypeCombo->setCurrentIndex(row->targetTypeCombo->findData(toInt(target)));

  row->targetValueCombo = new QComboBox(row->container);
  row->targetValueCombo->setEditable(true);
  row->targetValueCombo->setInsertPolicy(QComboBox::NoInsert);
  row->targetValueCombo->setMinimumWidth(220);
  if (auto* completer = row->targetValueCombo->completer()) {
    completer->setFilterMode(Qt::MatchContains);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
  }

  row->slotEdit = new QLineEdit(row->container);
  row->slotEdit->setPlaceholderText(tr("槽位键"));
  row->slotEdit->setMinimumWidth(120);

  row->addBtn = new QPushButton(QStringLiteral("+"), row->container);
  row->removeBtn = new QPushButton(QStringLiteral("-"), row->container);
  row->addBtn->setFixedWidth(28);
  row->removeBtn->setFixedWidth(28);
  row->addBtn->setToolTip(tr("添加关系行"));
  row->removeBtn->setToolTip(tr("移除该关系行"));

  layout->addWidget(row->kindCombo, 0);
  layout->addWidget(row->targetTypeCombo, 0);
  layout->addWidget(row->targetValueCombo, 1);
  layout->addWidget(row->slotEdit, 0);
  layout->addWidget(row->addBtn);
  layout->addWidget(row->removeBtn);

  auto* rowPtr = row.get();

  connect(rowPtr->addBtn, &QPushButton::clicked, this, &ModEditorDialog::onAddRelationRowClicked);
  connect(rowPtr->removeBtn, &QPushButton::clicked, this, [this, rowPtr]() { removeRelationRow(rowPtr); });
  connect(rowPtr->targetTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this, rowPtr](int) { refreshRelationRowChoices(rowPtr); });
  connect(rowPtr->kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this, rowPtr](int) {
            updateRelationRowKind(rowPtr);
            refreshRelationRowChoices(rowPtr);
          });

  if (insertIndex < 0 || insertIndex >= static_cast<int>(relationRows_.size())) {
    relationRowsLayout_->addWidget(rowPtr->container);
    relationRows_.push_back(std::move(row));
  } else {
    relationRowsLayout_->insertWidget(insertIndex, rowPtr->container);
    relationRows_.insert(relationRows_.begin() + insertIndex, std::move(row));
  }

  updateRelationRowKind(rowPtr);
  refreshRelationRowChoices(rowPtr);
  if (!value.isEmpty()) {
    const int idxValue = rowPtr->targetValueCombo->findText(value, Qt::MatchExactly);
    if (idxValue >= 0) {
      rowPtr->targetValueCombo->setCurrentIndex(idxValue);
    } else {
      rowPtr->targetValueCombo->setEditText(value);
    }
  } else {
    rowPtr->targetValueCombo->setCurrentIndex(0);
  }
  if (rowPtr->slotEdit) {
    rowPtr->slotEdit->setText(slot);
  }

  updateRelationRowRemoveButtons();
  return rowPtr;
}

void ModEditorDialog::removeRelationRow(RelationRowWidgets* row) {
  if (!row) {
    return;
  }
  auto it = std::find_if(relationRows_.begin(), relationRows_.end(),
                         [&](const std::unique_ptr<RelationRowWidgets>& holder) { return holder.get() == row; });
  if (it == relationRows_.end()) {
    return;
  }
  if (row->container) {
    relationRowsLayout_->removeWidget(row->container);
    row->container->deleteLater();
  }
  relationRows_.erase(it);
  updateRelationRowRemoveButtons();
}

void ModEditorDialog::clearRelationRows() {
  for (auto& row : relationRows_) {
    if (row && row->container) {
      relationRowsLayout_->removeWidget(row->container);
      row->container->deleteLater();
    }
  }
  relationRows_.clear();
  updateRelationRowRemoveButtons();
}

void ModEditorDialog::onAddRelationRowClicked() {
  auto* button = qobject_cast<QPushButton*>(sender());
  int insertIndex = -1;
  if (button) {
    for (int i = 0; i < static_cast<int>(relationRows_.size()); ++i) {
      if (relationRows_[i] && relationRows_[i]->addBtn == button) {
        insertIndex = i + 1;
        break;
      }
    }
  }
  auto* row = addRelationRow(RelationKind::Conflict,
                             RelationTarget::Mod,
                             QString(),
                             QString(),
                             insertIndex);
  if (row && row->targetValueCombo) {
    row->targetValueCombo->setFocus();
  }
}

void ModEditorDialog::updateRelationRowRemoveButtons() {
  const bool canRemove = relationRows_.size() > 1;
  for (auto& row : relationRows_) {
    if (row && row->removeBtn) {
      row->removeBtn->setEnabled(canRemove);
    }
  }
}

void ModEditorDialog::updateRelationRowKind(RelationRowWidgets* row) {
  if (!row || !row->kindCombo || !row->targetTypeCombo) {
    return;
  }
  const auto kind = relationKindFromData(row->kindCombo->currentData().toInt());
  const bool modOnly = (kind == RelationKind::Homologous || kind == RelationKind::CustomMaster ||
                        kind == RelationKind::CustomSlave);
  row->targetTypeCombo->setEnabled(!modOnly);
  if (modOnly) {
    const int index = row->targetTypeCombo->findData(toInt(RelationTarget::Mod));
    if (index >= 0) {
      row->targetTypeCombo->setCurrentIndex(index);
    }
  }

  const bool showSlot = (kind == RelationKind::CustomMaster || kind == RelationKind::CustomSlave);
  const bool slotEditable = (kind == RelationKind::CustomSlave);
  if (row->slotEdit) {
    row->slotEdit->setVisible(showSlot);
    row->slotEdit->setReadOnly(!slotEditable);
    if (!showSlot) {
      row->slotEdit->clear();
      return;
    }
    if (slotEditable) {
      row->slotEdit->setPlaceholderText(tr("槽位键（自定义从）"));
      row->slotEdit->setToolTip(tr("自定义从关系需要填写槽位键，用于区分不同槽位的从属 MOD。"));
    } else {
      row->slotEdit->setPlaceholderText(tr("槽位键（由自定义从填写）"));
      row->slotEdit->setToolTip(tr("当前为自定义（主）关系，槽位由对应的自定义（从）关系维护。"));
    }
  }
}

void ModEditorDialog::refreshRelationRowChoices(RelationRowWidgets* row) {
  if (!row || !row->targetValueCombo) {
    return;
  }

  const QString previousText = row->targetValueCombo->currentText().trimmed();
  const int previousId = row->targetValueCombo->currentData().toInt();

  QSignalBlocker blocker(row->targetValueCombo);
  row->targetValueCombo->clear();
  row->targetValueCombo->addItem(QString(), QVariant());
  row->targetValueCombo->setEditable(true);
  row->targetValueCombo->setInsertPolicy(QComboBox::NoInsert);

  const auto target = relationTargetFromData(row->targetTypeCombo->currentData().toInt());
  switch (target) {
    case RelationTarget::Mod: {
      for (const auto& mod : relationModOptions_) {
        const QString display =
            QStringLiteral("%1 (ID %2)").arg(QString::fromStdString(mod.name)).arg(mod.id);
        row->targetValueCombo->addItem(display, mod.id);
      }
      break;
    }
    case RelationTarget::Category: {
      for (const auto& cat : relationCategoryOptions_) {
        row->targetValueCombo->addItem(QString::fromStdString(cat.name), cat.id);
      }
      break;
    }
    case RelationTarget::Tag: {
      for (const QString& tag : relationTagOptions_) {
        row->targetValueCombo->addItem(tag, tag);
      }
      break;
    }
  }

  bool restored = false;
  if ((target == RelationTarget::Mod || target == RelationTarget::Category) && previousId > 0) {
    const int index = row->targetValueCombo->findData(previousId);
    if (index >= 0) {
      row->targetValueCombo->setCurrentIndex(index);
      restored = true;
    }
  }
  if (!restored && !previousText.isEmpty()) {
    const int index = row->targetValueCombo->findText(previousText, Qt::MatchFixedString);
    if (index >= 0) {
      row->targetValueCombo->setCurrentIndex(index);
    } else {
      row->targetValueCombo->setEditText(previousText);
    }
  } else if (!restored) {
    row->targetValueCombo->setCurrentIndex(0);
  }
}

std::vector<ModEditorDialog::RelationSelection> ModEditorDialog::selectedRelations() const {
  std::vector<RelationSelection> selections;
  for (const auto& row : relationRows_) {
    if (!row || !row->kindCombo || !row->targetTypeCombo || !row->targetValueCombo) {
      continue;
    }
    const QString value = row->targetValueCombo->currentText().trimmed();
    if (value.isEmpty()) {
      continue;
    }
    RelationSelection selection;
    selection.kind = relationKindFromData(row->kindCombo->currentData().toInt());
    selection.target = relationTargetFromData(row->targetTypeCombo->currentData().toInt());
    selection.targetId.reset();
    if (selection.target == RelationTarget::Mod || selection.target == RelationTarget::Category) {
      bool ok = false;
      const QVariant data = row->targetValueCombo->currentData();
      const int id = data.isValid() ? data.toInt(&ok) : 0;
      if (ok && id > 0) {
        selection.targetId = id;
      }
      selection.targetValue = selection.targetId.has_value() ? QString::number(*selection.targetId) : value;
    } else {
      selection.targetValue = value;
    }
    if (row->slotEdit) {
      selection.slotKey = row->slotEdit->text().trimmed();
    }
    selections.push_back(std::move(selection));
  }
  return selections;
}

std::vector<ModEditorDialog::RelationSelection> ModEditorDialog::relationSelections() const {
  // 封装内部选中结果，交由调用方执行进一步的关系落库逻辑。
  return selectedRelations();
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
  loadRelationSources();
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
  clearRelationRows();

  const auto relations = service_.listRelationsForMod(mod.id);
  if (relations.empty()) {
    addRelationRow();
  } else {
    for (const auto& rel : relations) {
      RelationKind kind;
      int targetModId = 0;
      bool isSymmetric = false;

      if (rel.type == "conflicts") {
        kind = RelationKind::Conflict;
        isSymmetric = true;
      } else if (rel.type == "requires") {
        if (rel.a_mod_id == mod.id) {
          kind = RelationKind::Requires;
          targetModId = rel.b_mod_id;
        } else {
          kind = RelationKind::RequiredBy;
          targetModId = rel.a_mod_id;
        }
      } else if (rel.type == "homologous") {
        kind = RelationKind::Homologous;
        isSymmetric = true;
      } else if (rel.type == "custom_master") {
        if (rel.a_mod_id == mod.id) {
          kind = RelationKind::CustomSlave;
          targetModId = rel.b_mod_id;
        } else if (rel.b_mod_id == mod.id) {
          kind = RelationKind::CustomMaster;
          targetModId = rel.a_mod_id;
        } else {
          continue;
        }
      } else {
        continue;
      }

      if (isSymmetric) {
        targetModId = (rel.a_mod_id == mod.id) ? rel.b_mod_id : rel.a_mod_id;
      }

      if (targetModId == 0) {
        continue;
      }

      QString targetValue;
      auto it = std::find_if(relationModOptions_.begin(), relationModOptions_.end(),
                             [targetModId](const ModRow& row) { return row.id == targetModId; });

      if (it != relationModOptions_.end()) {
        targetValue = QStringLiteral("%1 (ID %2)").arg(QString::fromStdString(it->name)).arg(it->id);
      } else {
        auto targetMod = service_.findMod(targetModId);
        if (targetMod) {
          targetValue = QStringLiteral("%1 (ID %2)").arg(QString::fromStdString(targetMod->name)).arg(targetMod->id);
        } else {
          targetValue = QStringLiteral("ID %1 (已删除)").arg(targetModId);
        }
      }

      const QString slot = QString::fromStdString(rel.slot_key.value_or(""));
      addRelationRow(kind, RelationTarget::Mod, targetValue, slot);
    }
  }
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

  const auto relations = selectedRelations();
  for (const auto& relation : relations) {
    if (relation.kind == RelationKind::CustomSlave && relation.slotKey.isEmpty()) {
      QMessageBox::warning(this, tr("缺少槽位键"), tr("自定义（从）关系需要填写槽位键。"));
      return;
    }
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
  const QString name =
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
  const QString groupName =
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

  // 如果文件名是纯数字，则默认推断为 Steam 工坊 ID 并自动补全链接与平台
  const bool isNumericId =
      !baseName.isEmpty() && std::all_of(baseName.cbegin(), baseName.cend(), [](const QChar& ch) { return ch.isDigit(); });
  if (isNumericId) {
    const QString steamPrefix = QStringLiteral("https://steamcommunity.com/sharedfiles/filedetails/?id=");
    const QString steamUrl = steamPrefix + baseName;
    if (sourceUrlEdit_) {
      QSignalBlocker blocker(sourceUrlEdit_);
      sourceUrlEdit_->setText(steamUrl);
    }
    if (sourcePlatformEdit_) {
      QSignalBlocker blocker(sourcePlatformEdit_);
      sourcePlatformEdit_->setText(QStringLiteral("steam"));
      platformEditedManually_ = false;
      lastAutoPlatform_ = QStringLiteral("steam");
    }
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
