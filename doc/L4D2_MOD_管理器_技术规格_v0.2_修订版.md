
# L4D2 MOD 管理器｜技术规格 v0.2（C++/Qt + SQLite）

## 概述
目标：做一个 **求生之路2（Left 4 Dead 2）** 的本地 MOD 管理器与“随机组合器”。
- **功能**：仓库化管理、评分/备注、分类+TAG、关系（前置/后置/冲突/同质/自定义主从/多人包）、导入/删除、游戏目录同步、随机策略组合、预览与冲突高亮。
- **平台**：Windows 10/11（优先）。
- **语言/框架**：C++20、Qt 6（Widgets 或 QML，默认 Widgets）、SQLite3、vcpkg 管理依赖；（可选）libcurl/QtNetwork、nlohmann/json、fmt、spdlog、cxxopts。
- **文件类型**：VPK/ZIP/7z/RAR（解压库先做 ZIP + VPK，7z/RAR 迭代）、Steam 创意工坊（workshop）。
- **目录结构**：MVP 使用单层目录（所有 MOD 文件统一位于 `repo_dir` 下）；后续版本扩展为按分类构建的二层目录镜像。
- **单位统一**：MOD 大小以 **MB（浮点）** 计，策略预算以 **MB** 计。

---
## 整体架构
```
app/                                 # GUI 入口（QtWidgets）
  ├─ main.cpp
  ├─ ui/                             # Qt UI 组件
  └─ viewmodel/                      # UI <-> 服务绑定
core/
  ├─ repo/RepositoryService.h/.cpp   # 仓库元数据 & 文件管理
  ├─ sync/GameSyncService.h/.cpp     # 仓库 <-> 游戏目录 同步
  ├─ import/ImportService.h/.cpp     # 文件/文件夹导入、Steam 页面抓取
  ├─ scan/Scanner.h/.cpp             # VPK/压缩包快速元数据扫描
  ├─ random/Randomizer.h/.cpp        # 随机策略引擎（含自定义主从槽位逻辑）
  ├─ relation/RelationService.h/.cpp # 前/后置、冲突、同质、主从（含槽位）、多人包
  ├─ tagging/TagService.h/.cpp       # 分类、TAG 管理
  ├─ config/Settings.h/.cpp          # 配置（Json）
  ├─ db/Db.h/.cpp                    # SQLite 访问层（DAO）
  ├─ log/Log.h/.cpp                  # spdlog 日志
  └─ util/                           # 工具：路径、hash、解压、网络、并发
3rdparty/                            # vcpkg 依赖或外部库
resources/                           # 图标/翻译/默认配置
```

---
## 数据库设计（SQLite）
> 使用 **版本化迁移**（`schema_version`），所有 DDL 存放于 `sql/migrations`。

### mods（核心）
```sql
CREATE TABLE IF NOT EXISTS mods (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,                  -- 与文件/文件夹名保持一致
  author TEXT,
  rating INTEGER CHECK(rating BETWEEN 1 AND 5),
  category_id INTEGER REFERENCES categories(id),
  note TEXT,
  published_at TEXT,                   -- ISO8601
  source TEXT,                         -- URL/来源平台标识
  is_deleted INTEGER NOT NULL DEFAULT 0,
  cover_path TEXT,
  file_path TEXT,                      -- 仓库存储路径（相对）
  file_hash TEXT,                      -- 内容哈希（去重）
  size_mb REAL NOT NULL DEFAULT 0.0,   -- 模组体积，MB（浮点）
  created_at TEXT NOT NULL DEFAULT (datetime('now')),
  updated_at TEXT NOT NULL DEFAULT (datetime('now')),
  UNIQUE(file_hash)
);
```

### categories / tag_groups / tags / mod_tags
```sql
CREATE TABLE IF NOT EXISTS categories (
  id INTEGER PRIMARY KEY,
  parent_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,
  name TEXT NOT NULL,
  UNIQUE(parent_id, name)
);

CREATE TABLE IF NOT EXISTS tag_groups (
  id INTEGER PRIMARY KEY,
  name TEXT UNIQUE NOT NULL,
  sort_order INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS tags (
  id INTEGER PRIMARY KEY,
  group_id INTEGER NOT NULL REFERENCES tag_groups(id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  UNIQUE(group_id, name)
);
CREATE TABLE IF NOT EXISTS mod_tags (
  mod_id INTEGER NOT NULL REFERENCES mods(id) ON DELETE CASCADE,
  tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
  PRIMARY KEY(mod_id, tag_id)
);
```

### mod_relations（统一关系，含自定义主从槽位）
```sql
-- type: 'requires' | 'conflicts' | 'homologous' | 'custom_master' | 'party'
CREATE TABLE IF NOT EXISTS mod_relations (
  id INTEGER PRIMARY KEY,
  a_mod_id INTEGER NOT NULL REFERENCES mods(id) ON DELETE CASCADE,
  b_mod_id INTEGER NOT NULL REFERENCES mods(id) ON DELETE CASCADE,
  type TEXT NOT NULL,
  slot_key TEXT,                      -- 仅 custom_master 使用
  note TEXT,
  CHECK(a_mod_id <> b_mod_id),
  UNIQUE(a_mod_id, b_mod_id, type)
);
CREATE INDEX IF NOT EXISTS idx_mod_rel_a    ON mod_relations(a_mod_id);
CREATE INDEX IF NOT EXISTS idx_mod_rel_b    ON mod_relations(b_mod_id);
CREATE INDEX IF NOT EXISTS idx_mod_rel_type ON mod_relations(type);
```

### saved_schemes / strategies
```sql
CREATE TABLE IF NOT EXISTS saved_schemes (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  budget_mb REAL NOT NULL DEFAULT 2048.0,
  created_at TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS saved_scheme_items (
  scheme_id INTEGER NOT NULL REFERENCES saved_schemes(id) ON DELETE CASCADE,
  mod_id INTEGER NOT NULL REFERENCES mods(id) ON DELETE CASCADE,
  is_locked INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY(scheme_id, mod_id)
);

CREATE TABLE IF NOT EXISTS fixed_bundles (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  note TEXT
);
CREATE TABLE IF NOT EXISTS fixed_bundle_items (
  bundle_id INTEGER NOT NULL REFERENCES fixed_bundles(id) ON DELETE CASCADE,
  mod_id INTEGER NOT NULL REFERENCES mods(id) ON DELETE CASCADE,
  PRIMARY KEY(bundle_id, mod_id)
);

CREATE TABLE IF NOT EXISTS strategies (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  json TEXT NOT NULL,
  updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);
```

- `fixed_bundles`：固定搭配。导入器载入后会自动选中并锁定其中的 MOD。
- `saved_schemes`：组合方案。导入器载入后仅选中 MOD，是否锁定由 `saved_scheme_items.is_locked` 决定。
- 随机组合器生成的临时结果不会立即写入以上表，需要玩家确认后才执行保存。

### 视图
```sql
CREATE VIEW IF NOT EXISTS v_mods_visible AS
SELECT * FROM mods WHERE is_deleted = 0;
```

---
## 策略 JSON Schema
```json
{
  "scope": {
    "include_categories": ["人物", "枪械", "特感"],
    "exclude_categories": ["环境", "地图"],
    "include_tags": [
      { "group": "二次元", "tag": "明日方舟" }
    ],
    "exclude_tags": []
  },
  "priorities": {
    "character_full_set": true,
    "weapon_full_set": "common"
  },
  "rating": {
    "mode": "prefer_min",
    "threshold": 4
  },
  "traversal": {
    "enable": true,
    "window": 50
  },
  "budget": { "max_mb": 2048.0 },
  "custom_parts": { "enable": true, "max_per_slot": 1 },
  "slot_tiebreaker": "rating_desc|size_mb_asc|last_used_asc",
  "tie_breaker": "rating_desc|name_asc"
}
```

---
## 导入流程与阶段
- **阶段 1（MVP）**：仅读取文件名、创建/修改时间、大小（MB）等可直接获取的信息。  
- **阶段 2**：增加 VPK 元信息解析/轻量解包（addoninfo、作者、封面、适用角色）。  
- **阶段 3**（可选）：支持 ZIP/7z/RAR 压缩包扫描与解压。

接口：
```cpp
struct TagSelection {
  std::string group;
  std::string tag;
};

struct ImportOptions {
  bool move_files;
  std::optional<int> category_id;
  std::vector<TagSelection> tags;
};
ModId importFile(const fs::path& file, const ImportOptions& opt);
std::vector<ModId> importFolder(const fs::path& folder, const ImportOptions& opt);
```

---
## 关系与主从槽位逻辑
- **单向关系**：`custom_master(A,B,slot_key)`；A 为主、B 为从。  
- **槽位识别**：从名称推测 slot_key（如 `xxx-a1`、`xxx-b1`）。  
- **互斥规则**：同主下，同一 slot_key 组最多选 1 个从；从被选中时必须包含主。  
- **显示排序**：主置顶，从按槽位→评分降序→名称升序排列。

接口摘要：
```cpp
struct SlaveGroup { std::string slot_key; std::vector<int> slaves; };
std::vector<SlaveGroup> getCustomSlavesBySlot(int master_id);
bool hasCustomMaster(int mod_id);
int getMasterOf(int slave_id);
```

---
## 服务模块要点
### RepositoryService
管理 MOD CRUD、逻辑删除、查询过滤与排序；支持显示/隐藏已删除项。

### RelationService
维护前置、冲突、同质、自定义主从、多人包关系；提供冲突报告。

### Randomizer
按策略生成组合：范围过滤、评分阈值、预算、槽位互斥、依赖闭包、遍历优先。

### GameSyncService
负责仓库与游戏目录同步：扫描已有 MOD、生成差异计划、执行复制/删除。

### ImportService
处理文件/文件夹导入：计算哈希、记录体积、推测分类/TAG。

---
## UI 映射
- **仓库总览**：名称、分类、TAG、作者、评分、备注、来源、日期。右侧封面+备注；支持筛选与显示已删除项。  
- **选择器界面**：左=游戏已启用 MOD，右=仓库候选；红=冲突、黄=前置、绿=后置或附属建议；支持随机组合、保存组合、锁定条目。

---
## 配置与存储
- `settings.json`：记录仓库路径、游戏路径、显示列、移动文件选项、显示已删除项。  
- 分类/TAG 在数据库中管理，支持 JSON 初始化与导入导出：
  ```json
  {
    "categories": {
      "人物": ["Nick", "Rochelle", "Coach", "Ellis"],
      "枪械": ["M16", "AK47", "M60"]
    },
    "tag_groups": {
      "二次元": ["VRC", "明日方舟", "崩坏", "BA", "碧蓝航线", "虚拟主播"],
      "三次元": ["军事风"],
      "健全度": ["健全", "非健全"],
      "获取方式": ["免费", "付费", "定制"]
    }
  }
  ```
  首次启动可读取 `init_categories.json` 与 `init_tags.json` 自动填充数据库。

---
## 核心算法
1. **同质互斥**：同质集合建立 slot，随机时每 slot 仅取 1 个。  
2. **冲突检测**：构建无向图检测互斥，按优先级（锁定>评分>最近使用）剔除。  
3. **前置闭包**：依赖自动补齐，超预算则回退评分最低或体积最大项。  
4. **遍历优先**：维护 `last_used_at` 与 `use_count`，冷门优先。

---
## 迭代计划（项目开发阶段）
### 阶段 1：MVP（首个可运行版本）
- SQLite + DAO 初始化。  
- 导入文件/文件夹（基础信息）。  
- 仓库列表 + 评分/备注编辑。  
- 游戏目录同步（复制/删除）。  
- 前置/冲突关系。  
- 随机策略（人物/枪械/特感 + 评分阈值 + 预算）。  
- 组合保存与锁定。  
- 分类/TAG 管理 + JSON 导入导出。  

### 阶段 2：v0.2（功能扩展）
- 封面缓存与展示。  
- Steam 页面抓取 MOD 信息。  
- 自定义主从槽位逻辑完善。  
- 遍历优先机制。  
- 策略编辑器 UI。  
- **TAG 二级分类与类别特定 TAG**：引入 tag_groups 与 tag_scope 表结构，为不同分类定义可用 TAG。

### 阶段 3：v0.3（增强与优化）
- 支持 7z/RAR 压缩格式。  
- 二层目录镜像与文件监控。  
- 冲突解释器 UI。  
- 统计分析模块（使用频率、存储体积占比等）。  

---
## 测试清单
- 导入：重复文件、同名不同内容、哈希冲突、大文件中断。  
- 关系：环依赖、缺前置、同槽互斥、冲突提示。  
- 同步：预算边界、磁盘不足、路径错误、只读目录。  
- 随机：锁定覆盖、评分阈值退化、遍历权重。  

---
## 风险与对策
- **VPK/压缩包解析**：仅实现元信息阶段，完整解包延后。  
- **Steam 页面反爬**：优先使用官方 API PublishedFileDetails。  
- **路径与权限**：复制前检测空间与权限，路径采用短名与相对路径策略。

---
