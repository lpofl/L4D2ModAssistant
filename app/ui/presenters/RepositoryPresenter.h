#pragma once

#include <QObject>
#include <QString>
#include <vector>
#include <unordered_map>

class QCheckBox;
class QComboBox;
class QLabel;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTextEdit;
class QWidget;

struct CategoryRow;
struct ModRow;
struct TagDescriptor;
struct TagWithGroupRow;

class ImportService;
class RepositoryPage;
class RepositoryService;
class ModFilterPanel;
class ModTableWidget;
struct Settings;

/**
 * @brief Orchestrates repository page behaviour: data loading, filtering, CRUD.
 */
class RepositoryPresenter : public QObject {
  Q_OBJECT

public:
  RepositoryPresenter(RepositoryPage* page,
                      Settings& settings,
                      QWidget* dialogParent,
                      QObject* parent = nullptr);

  void setRepositoryService(RepositoryService* repo);
  void setImportService(ImportService* service);
  void setRepositoryDirectory(const QString& path);

  void initializeFilters();
  void reloadAll();

  const std::vector<ModRow>& mods() const { return mods_; }

  QString tagsTextForMod(int modId) const;
  std::vector<TagDescriptor> tagsForMod(int modId) const;
  QString categoryNameFor(int categoryId) const;

  bool modMatchesFilter(const ModRow& mod,
                        const QString& attribute,
                        int filterId,
                        const QString& filterValue) const;

  void populateCategoryFilterModel(QStandardItemModel* model, bool updateCache);
  void populateTagFilterModel(QStandardItemModel* model) const;
  void populateAuthorFilterModel(QStandardItemModel* model) const;
  void populateRatingFilterModel(QStandardItemModel* model) const;

  void updateDetailForMod(int modId);

  int filterIdForCombo(const QComboBox* combo,
                       const QSortFilterProxyModel* proxy,
                       const QStandardItemModel* model) const;

signals:
  void modsReloaded();

private slots:
  void handleRefreshRequested();
  void handleImportRequested();
  void handleEditRequested();
  void handleDeleteRequested();
  void handleShowDeletedToggled(bool checked);
  void handleFilterAttributeChanged(const QString& attribute);
  void handleFilterValueTextChanged(const QString& text);
  void handleFilterChanged(const QString& text);
  void handleCurrentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn);

private:
  void loadData();
  void populateTable();
  void reloadCategories();
  void reloadTags();
  void reloadAuthors();
  void reloadRatings();

  QString formatTagSummary(const std::vector<TagWithGroupRow>& rows,
                           const QString& groupSeparator,
                           const QString& tagSeparator) const;
  bool categoryMatchesFilter(int modCategoryId, int filterCategoryId) const;

  RepositoryPage* page_{};
  RepositoryService* repo_{};
  ImportService* importService_{};
  Settings* settings_{};
  QWidget* dialogParent_{};
  QString repoDir_;

  ModFilterPanel* filterPanel_{};
  QComboBox* filterAttribute_{};
  QComboBox* filterValue_{};
  QCheckBox* showDeletedCheckBox_{};
  ModTableWidget* modTable_{};
  QLabel* coverLabel_{};
  QLabel* metaLabel_{};
  QTextEdit* noteView_{};

  QStandardItemModel* filterModel_{};
  QSortFilterProxyModel* filterProxy_{};

  std::vector<ModRow> mods_;
  std::unordered_map<int, QString> categoryNames_;
  std::unordered_map<int, int> categoryParent_;
  std::unordered_map<int, QString> modTagsText_;
  std::unordered_map<int, std::vector<TagWithGroupRow>> modTagsCache_;
  bool suppressFilterSignals_ = false;
};

