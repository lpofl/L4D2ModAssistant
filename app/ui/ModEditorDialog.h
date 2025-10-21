#pragma once

#include <QDialog>
#include <QString>
#include <memory>
#include <vector>

#include "core/repo/RepositoryService.h"

class QLineEdit;
class QComboBox;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QPlainTextEdit;
class QTreeWidget;
class QDialogButtonBox;
class QTreeWidgetItem;
class QFileInfo;
class QString;

/**
 * @brief Dialog used for creating or editing a MOD entry plus tag assignments.
 */
class ModEditorDialog : public QDialog {
  Q_OBJECT
public:
  explicit ModEditorDialog(RepositoryService& service, QWidget* parent = nullptr);

  /// Populate dialog with existing mod data.
  void setMod(const ModRow& mod, const std::vector<TagDescriptor>& tags);

  /// Collect user input into a ModRow (id preserved).
  ModRow modData() const;

  /// Gather tag descriptors chosen/added by the user.
  std::vector<TagDescriptor> selectedTags() const;

protected:
  void accept() override;

private slots:
  void onBrowseFile();
  void onBrowseCover();
  void onAddCategory();
  void onAddTag();
  void onFilePathEdited(const QString& path);
  void onSourceUrlEdited(const QString& url);
  void onSourcePlatformEdited(const QString& text);

private:
  void buildUi();
  void loadCategories();
  void loadTags();
  void setCheckedTags(const std::vector<TagDescriptor>& tags);
  QTreeWidgetItem* ensureGroupItem(const QString& groupName);
  void applyFileMetadata(const QString& path);
  QString locateCoverSibling(const QFileInfo& fileInfo) const;
  void maybeAutoFillPlatform(const QString& url);

  RepositoryService& service_;
  int modId_{0};
  bool suppressFileSignal_{false};
  bool platformEditedManually_{false};
  QString lastAutoPlatform_;

  QLineEdit* nameEdit_{};
  QLineEdit* authorEdit_{};
  QComboBox* categoryCombo_{};
  QPushButton* addCategoryBtn_{};
  QSpinBox* ratingSpin_{};
  QDoubleSpinBox* sizeSpin_{};
  QLineEdit* publishedEdit_{};
  QLineEdit* sourcePlatformEdit_{};
  QLineEdit* sourceUrlEdit_{};
  QLineEdit* filePathEdit_{};
  QPushButton* browseFileBtn_{};
  QLineEdit* coverPathEdit_{};
  QPushButton* browseCoverBtn_{};
  QLineEdit* hashEdit_{};
  QPlainTextEdit* noteEdit_{};
  QTreeWidget* tagTree_{};
  QComboBox* tagGroupCombo_{};
  QLineEdit* newTagEdit_{};
  QPushButton* addTagBtn_{};
  QDialogButtonBox* buttonBox_{};

  std::vector<CategoryRow> categories_;
  std::vector<TagGroupRow> tagGroups_;
};
