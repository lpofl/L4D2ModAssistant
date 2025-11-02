#include "app/ui/presenters/SelectorPresenter.h"

#include <algorithm>

#include <QComboBox>
#include <QFont>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableWidgetItem>
#include <unordered_map>

#include "app/ui/components/ModFilterPanel.h"
#include "app/ui/components/ModTableWidget.h"
#include "app/ui/pages/SelectorPage.h"
#include "app/ui/presenters/RepositoryPresenter.h"
#include "core/repo/GameModDao.h"
#include "core/repo/RepositoryService.h"

namespace {

QString toDisplay(const std::string& value, const QString& fallback = {}) {
  return value.empty() ? fallback : QString::fromStdString(value);
}

} // namespace

SelectorPresenter::SelectorPresenter(SelectorPage* page, QObject* parent)
    : QObject(parent), page_(page) {
  if (page_) {
    filterPanel_ = page_->filterPanel();
    if (filterPanel_) {
      filterAttribute_ = filterPanel_->attributeCombo();
      filterValue_ = filterPanel_->valueCombo();
    }
    gameDirTable_ = page_->gameDirectoryTable();
    repoTable_ = page_->repositoryTable();

    connect(page_, &SelectorPage::filterAttributeChanged, this, &SelectorPresenter::handleFilterAttributeChanged);
    connect(page_, &SelectorPage::filterValueChanged, this, &SelectorPresenter::handleFilterValueChanged);
    connect(page_, &SelectorPage::filterValueTextChanged, this, &SelectorPresenter::handleFilterValueTextChanged);
  }

  filterModel_ = new QStandardItemModel(this);
  filterProxy_ = new QSortFilterProxyModel(this);
  filterProxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
  filterProxy_->setFilterKeyColumn(0);
  if (filterPanel_) {
    filterPanel_->setValueModels(filterModel_, filterProxy_);
  }
}

void SelectorPresenter::initializeFilters() {
  // 初始化筛选项与占位提示
  if (!filterAttribute_) {
    return;
  }
  filterAttribute_->clear();
  filterAttribute_->addItems({tr("名称"), tr("分类"), tr("标签"), tr("作者"), tr("评分")});
  filterAttribute_->setCurrentText(tr("名称"));

  if (filterValue_) {
    if (auto* line = filterValue_->lineEdit()) {
      line->setClearButtonEnabled(true);
      line->setPlaceholderText(tr("搜索名称"));
    }
  }
}

void SelectorPresenter::setRepositoryPresenter(RepositoryPresenter* presenter) {
  repositoryPresenter_ = presenter;
}

void SelectorPresenter::setRepositoryService(RepositoryService* service) {
  // 保留仓库服务指针，后续刷新游戏目录列表时需要访问数据库缓存
  repoService_ = service;
  refreshGameDirectory();
}

void SelectorPresenter::refreshRepositoryData() {
  if (repoService_) {
    // ��ˢ����ϷĿ¼�����Ա�֤ UI ��ʾΪ����ɨ���
    refreshGameDirectory();
  }
  // 仓库数据刷新后联动更新筛选视图
  if (!filterAttribute_) {
    return;
  }
  handleFilterAttributeChanged(filterAttribute_->currentText());
}


void SelectorPresenter::refreshGameDirectory() {
  if (!gameDirTable_) {
    return;
  }
  gameDirTable_->setRowCount(0);
  if (!repoService_) {
    return;
  }

  const QString currentAttribute = filterAttribute_ ? filterAttribute_->currentText() : QString();
  const QString currentFilterValue = filterValue_ ? filterValue_->currentText() : QString();
  const int currentFilterId = filterIdForCombo(filterValue_, filterProxy_, filterModel_);

  // 通过仓库缓存表读取游戏目录扫描结果，并匹配仓库 MOD 补全展示信息
  const std::vector<GameModRow> gameMods = repoService_->listGameMods();
  std::unordered_map<int, const ModRow*> repoIndex;
  if (repositoryPresenter_) {
    const auto& mods = repositoryPresenter_->mods();
    repoIndex.reserve(mods.size());
    for (const auto& mod : mods) {
      repoIndex.emplace(mod.id, &mod);
    }
  }

  gameDirTable_->setRowCount(static_cast<int>(gameMods.size()));
  int rowIndex = 0;
  for (const auto& cacheRow : gameMods) {
    const int repoId = cacheRow.repo_mod_id.value_or(0);
    const ModRow* repoMod = nullptr;
    if (repoId > 0) {
      const auto it = repoIndex.find(repoId);
      if (it != repoIndex.end()) {
        repoMod = it->second;
      }
    }

    QString displayName = repoMod ? QString::fromStdString(repoMod->name)
                                  : QString::fromStdString(cacheRow.name);
    auto* nameItem = new QTableWidgetItem(displayName);
    nameItem->setData(Qt::UserRole, repoMod ? repoMod->id : 0);
    const bool isWorkshop = QString::fromStdString(cacheRow.source) == QStringLiteral("workshop");
    if (isWorkshop) {
      QFont font = nameItem->font();
      font.setItalic(true);
      nameItem->setFont(font);
    }

    const double sizeMb = cacheRow.file_size > 0
                              ? static_cast<double>(cacheRow.file_size) / (1024.0 * 1024.0)
                              : 0.0;
    const QString tooltip = tr("文件路径: %1\n文件大小: %2 MB\n修改时间: %3")
                                .arg(QString::fromStdString(cacheRow.file_path))
                                .arg(QString::number(sizeMb, 'f', 2))
                                .arg(QString::fromStdString(cacheRow.modified_at));
    nameItem->setToolTip(tooltip);
    gameDirTable_->setItem(rowIndex, 0, nameItem);

    const QString tagsText = (repoMod && repositoryPresenter_)
                                 ? repositoryPresenter_->tagsTextForMod(repoMod->id)
                                 : QString();
    gameDirTable_->setItem(rowIndex, 1, new QTableWidgetItem(tagsText));

    const QString authorText = repoMod ? QString::fromStdString(repoMod->author) : QString();
    gameDirTable_->setItem(rowIndex, 2, new QTableWidgetItem(authorText.isEmpty() ? QStringLiteral("-") : authorText));

    QString ratingText = QStringLiteral("-");
    if (repoMod && repoMod->rating > 0) {
      ratingText = QString::number(repoMod->rating);
    }
    gameDirTable_->setItem(rowIndex, 3, new QTableWidgetItem(ratingText));

    const QString noteText = repoMod ? QString::fromStdString(repoMod->note) : QString();
    gameDirTable_->setItem(rowIndex, 4, new QTableWidgetItem(noteText));

    auto* statusItem = new QTableWidgetItem(QString::fromStdString(cacheRow.status));
    statusItem->setData(Qt::UserRole, QString::fromStdString(cacheRow.source));
    gameDirTable_->setItem(rowIndex, 5, statusItem);

    ++rowIndex;
  }

  updateGameDirVisibility(currentAttribute, currentFilterId, currentFilterValue);
}

void SelectorPresenter::applyFilter() {
  // 基于当前筛选条件重建选择器表格
  if (!repoTable_ || !filterAttribute_ || !filterValue_ || !repositoryPresenter_) {
    return;
  }

  const QString attribute = filterAttribute_->currentText();
  const QString filterValueText = filterValue_->currentText();
  const int filterId = filterIdForCombo(filterValue_, filterProxy_, filterModel_);

  repoTable_->setRowCount(0);
  const auto& mods = repositoryPresenter_->mods();
  int row = 0;
  for (const auto& mod : mods) {
    if (mod.is_deleted) {
      continue;
    }
    if (!repositoryPresenter_->modMatchesFilter(mod, attribute, filterId, filterValueText)) {
      continue;
    }

    repoTable_->insertRow(row);
    auto* itemName = new QTableWidgetItem(QString::fromStdString(mod.name));
    itemName->setData(Qt::UserRole, mod.id);
    repoTable_->setItem(row, 0, itemName);
    repoTable_->setItem(row, 1, new QTableWidgetItem(repositoryPresenter_->tagsTextForMod(mod.id)));
    repoTable_->setItem(row, 2, new QTableWidgetItem(toDisplay(mod.author)));
    repoTable_->setItem(row, 3, new QTableWidgetItem(mod.rating > 0 ? QString::number(mod.rating) : QString("-")));
    repoTable_->setItem(row, 4, new QTableWidgetItem(toDisplay(mod.note)));
    ++row;
  }

  updateGameDirVisibility(attribute, filterId, filterValueText);
}


void SelectorPresenter::handleFilterAttributeChanged(const QString& attribute) {
  if (!filterModel_ || !filterProxy_ || !filterValue_ || !repositoryPresenter_) {
    return;
  }

  suppressFilterSignals_ = true;
  filterValue_->blockSignals(true);

  filterModel_->clear();
  filterProxy_->setFilterFixedString(QString());

  QString placeholder;
  if (attribute == tr("名称")) {
    placeholder = tr("搜索名称");
  } else if (attribute == tr("分类")) {
    repositoryPresenter_->populateCategoryFilterModel(filterModel_, false);
    placeholder = tr("选择分类");
  } else if (attribute == tr("标签")) {
    repositoryPresenter_->populateTagFilterModel(filterModel_);
    placeholder = tr("选择标签");
  } else if (attribute == tr("作者")) {
    repositoryPresenter_->populateAuthorFilterModel(filterModel_);
    placeholder = tr("选择作者");
  } else if (attribute == tr("评分")) {
    repositoryPresenter_->populateRatingFilterModel(filterModel_);
    placeholder = tr("选择评分");
  }

  filterValue_->setModel(nullptr);
  filterValue_->setModel(filterProxy_);
  filterValue_->setEditText(QString());
  filterValue_->setCurrentIndex(-1);

  if (auto* edit = filterValue_->lineEdit()) {
    edit->setPlaceholderText(placeholder);
  }

  filterValue_->blockSignals(false);
  suppressFilterSignals_ = false;

  applyFilter();
}

void SelectorPresenter::handleFilterValueChanged(const QString& /*text*/) {
  if (!suppressFilterSignals_) {
    applyFilter();
  }
}

void SelectorPresenter::handleFilterValueTextChanged(const QString& text) {
  if (!filterProxy_ || suppressFilterSignals_) {
    return;
  }
  filterProxy_->setFilterFixedString(text);

  if (filterAttribute_ && filterValue_ && filterValue_->lineEdit()) {
    const QString currentFilter = filterAttribute_->currentText();
    if ((currentFilter == tr("名称") || currentFilter == tr("标签")) && filterValue_->lineEdit()->hasFocus()) {
      filterValue_->showPopup();
    }
  }
}

int SelectorPresenter::filterIdForCombo(const QComboBox* combo,
                                        const QSortFilterProxyModel* proxy,
                                        const QStandardItemModel* model) const {
  if (!combo || !proxy || !model) {
    return 0;
  }
  const int index = combo->currentIndex();
  if (index < 0) {
    return 0;
  }
  const QModelIndex proxyIndex = proxy->index(index, 0);
  if (!proxyIndex.isValid()) {
    return 0;
  }
  const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);
  if (!sourceIndex.isValid()) {
    return 0;
  }
  return model->data(sourceIndex, Qt::UserRole).toInt();
}

// 根据筛选条件同步游戏目录列表的可见状态
void SelectorPresenter::updateGameDirVisibility(const QString& attribute,
                                                int filterId,
                                                const QString& filterValueText) {
  if (!gameDirTable_ || !repositoryPresenter_) {
    return;
  }

  const auto& mods = repositoryPresenter_->mods();
  const int rowCount = gameDirTable_->rowCount();
  for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
    bool visible = true;
    if (auto* item = gameDirTable_->item(rowIndex, 0)) {
      const int modId = item->data(Qt::UserRole).toInt();
      if (modId > 0) {
        const auto it = std::find_if(mods.begin(), mods.end(), [modId](const ModRow& row) {
          return row.id == modId;
        });
        if (it != mods.end()) {
          visible = repositoryPresenter_->modMatchesFilter(*it, attribute, filterId, filterValueText);
        }
      }
    }
    gameDirTable_->setRowHidden(rowIndex, !visible);
  }
}

