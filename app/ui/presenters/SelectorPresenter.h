#pragma once

#include <QObject>
#include <QString>

class QComboBox;
class QSortFilterProxyModel;
class QStandardItemModel;

class ModFilterPanel;
class ModTableWidget;
class RepositoryPresenter;
class SelectorPage;
class RepositoryService;

/**
 * @brief 选择器页业务协调器，负责过滤条件与仓库数据的衔接。
 */
class SelectorPresenter : public QObject {
  Q_OBJECT
public:
  explicit SelectorPresenter(SelectorPage* page, QObject* parent = nullptr);

  void initializeFilters();
  void setRepositoryPresenter(RepositoryPresenter* presenter);
  void setRepositoryService(RepositoryService* service);
  void refreshRepositoryData();
  void applyFilter();
  void refreshGameDirectory();

private slots:
  void handleFilterAttributeChanged(const QString& attribute);
  void handleFilterValueChanged(const QString& text);
  void handleFilterValueTextChanged(const QString& text);

private:
  int filterIdForCombo(const QComboBox* combo,
                       const QSortFilterProxyModel* proxy,
                       const QStandardItemModel* model) const;
  void updateGameDirVisibility(const QString& attribute, int filterId, const QString& filterValueText);

  SelectorPage* page_{};
  RepositoryPresenter* repositoryPresenter_{};
  RepositoryService* repoService_{};
  ModFilterPanel* filterPanel_{};
  QComboBox* filterAttribute_{};
  QComboBox* filterValue_{};
  ModTableWidget* repoTable_{};
  ModTableWidget* gameDirTable_{};
  QStandardItemModel* filterModel_{};
  QSortFilterProxyModel* filterProxy_{};
  bool suppressFilterSignals_ = false;
};

