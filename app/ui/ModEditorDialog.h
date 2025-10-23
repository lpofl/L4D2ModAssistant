#pragma once

#include <QDialog>
#include <QString>
#include <QMap>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <QStringList>

#include "core/config/AttributeOptions.h"

#include "core/repo/RepositoryService.h"

class QLineEdit;
class QComboBox;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QPlainTextEdit;
class QDialogButtonBox;
class QFileInfo;
class QVBoxLayout;
class QHBoxLayout;
class QWidget;

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
  void onPrimaryCategoryChanged(int index);
  void onAddTagRowClicked();
  void onAddRelationRowClicked();

private:
  struct TagRowWidgets {
    QWidget* container{nullptr};
    QComboBox* groupCombo{nullptr};
    QComboBox* tagCombo{nullptr};
    QPushButton* addBtn{nullptr};
    QPushButton* removeBtn{nullptr};
  };

  enum class RelationKind {
    Conflict,
    Requires,
    RequiredBy,
    Homologous,
    CustomMaster,
    CustomSlave,
    Party
  };

  enum class RelationTarget {
    Mod,
    Category,
    Tag
  };

  struct RelationRowWidgets {
    QWidget* container{nullptr};
    QComboBox* kindCombo{nullptr};
    QComboBox* targetTypeCombo{nullptr};
    QComboBox* targetValueCombo{nullptr};
    QLineEdit* slotEdit{nullptr};
    QPushButton* addBtn{nullptr};
    QPushButton* removeBtn{nullptr};
  };

  struct RelationSelection {
    RelationKind kind{RelationKind::Conflict};
    RelationTarget target{RelationTarget::Mod};
    QString targetValue;
    QString slotKey;
  };

  void buildUi();
  void loadCategories();
  void loadTags();
  void setCheckedTags(const std::vector<TagDescriptor>& tags);
  void applyFileMetadata(const QString& path);
  QString locateCoverSibling(const QFileInfo& fileInfo) const;
  void maybeAutoFillPlatform(const QString& url);
  void rebuildSecondaryCategories(std::optional<int> parentId);
  TagRowWidgets* addTagRow(const QString& group = {}, const QString& tag = {}, int insertIndex = -1);
  void removeTagRow(TagRowWidgets* row);
  void refreshTagChoices(TagRowWidgets* row);
  void setupSearchableCombo(QComboBox* combo, const QString& placeholder = {});
  void clearTagRows();
  void updateTagRowRemoveButtons();
  void loadAttributeOptions();
  void loadRelationSources();
  RelationRowWidgets* addRelationRow(RelationKind kind = RelationKind::Conflict,
                                     RelationTarget target = RelationTarget::Mod,
                                     const QString& value = {},
                                     const QString& slot = {},
                                     int insertIndex = -1);
  void removeRelationRow(RelationRowWidgets* row);
  void refreshRelationRowChoices(RelationRowWidgets* row);
  void clearRelationRows();
  void updateRelationRowRemoveButtons();
  void updateRelationRowKind(RelationRowWidgets* row);
  std::vector<RelationSelection> selectedRelations() const;
  static RelationKind relationKindFromData(int value);
  static RelationTarget relationTargetFromData(int value);
  static int toInt(RelationKind kind);
  static int toInt(RelationTarget target);

  RepositoryService& service_;
  int modId_{0};
  bool suppressFileSignal_{false};
  bool platformEditedManually_{false};
  QString lastAutoPlatform_;

  QLineEdit* nameEdit_{};
  QLineEdit* authorEdit_{};
  QComboBox* primaryCategoryCombo_{};
  QComboBox* secondaryCategoryCombo_{};
  QPushButton* addCategoryBtn_{};
  QSpinBox* ratingSpin_{};
  QDoubleSpinBox* sizeSpin_{};
  QLineEdit* lastPublishedEdit_{};
  QLineEdit* lastSavedEdit_{};
  QComboBox* statusCombo_{};
  QComboBox* integrityCombo_{};
  QComboBox* stabilityCombo_{};
  QComboBox* acquisitionCombo_{};
  QLineEdit* sourcePlatformEdit_{};
  QLineEdit* sourceUrlEdit_{};
  QLineEdit* filePathEdit_{};
  QPushButton* browseFileBtn_{};
  QLineEdit* coverPathEdit_{};
  QPushButton* browseCoverBtn_{};
  QLineEdit* hashEdit_{};
  QPlainTextEdit* noteEdit_{};
  QWidget* tagRowsContainer_{};
  QVBoxLayout* tagRowsLayout_{};
  QPushButton* addTagRowButton_{};
  QPushButton* manageTagsButton_{};
  std::vector<std::unique_ptr<TagRowWidgets>> tagRows_;
  QWidget* relationRowsContainer_{};
  QVBoxLayout* relationRowsLayout_{};
  std::vector<std::unique_ptr<RelationRowWidgets>> relationRows_;
  QDialogButtonBox* buttonBox_{};

  std::vector<CategoryRow> categories_;
  std::vector<CategoryRow> primaryCategories_;
  std::unordered_map<int, std::vector<CategoryRow>> secondaryCategories_;
  std::vector<TagGroupRow> tagGroups_;
  QMap<QString, QStringList> tagItemsByGroup_;
  config::ModAttributeOptions attributeOptions_;
  std::vector<ModRow> relationModOptions_;
  std::vector<CategoryRow> relationCategoryOptions_;
  QStringList relationTagOptions_;
};
