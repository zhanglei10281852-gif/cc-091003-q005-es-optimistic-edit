#ifndef ES_CLIENT_HPP
#define ES_CLIENT_HPP

#include "http_client.hpp"
#include "json.hpp"
#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace es {

using json = nlohmann::json;

/**
 * 搜索命中结果
 */
struct SearchHit {
    std::string id;
    std::string index;
    double score;
    json source;
    json highlight;
};

/**
 * 搜索结果
 */
struct SearchResult {
    int total;
    double maxScore;
    std::vector<SearchHit> hits;
    int took;  // 耗时（毫秒）
    bool timedOut;
};

/**
 * 文档操作结果
 */
struct DocResult {
    std::string id;
    std::string index;
    std::string result;  // created, updated, deleted
    int version;
    bool success;
};

/**
 * 批量操作结果
 */
struct BulkResult {
    int took;
    bool errors;
    std::vector<DocResult> items;
    int successCount;
    int failCount;
};

/**
 * 编辑凭据（乐观并发控制令牌）
 *
 * 读取文档时由 Elasticsearch 返回的 _seq_no 与 _primary_term 对。
 * 交互式编辑场景下，更新/删除必须原样带回该凭据：
 * 仅当服务端文档自读取后未被他人修改，写入才会成功。
 */
struct EditCredential {
    long long seqNo = -1;       ///< 对应 ES 的 _seq_no（>= 0 才有效）
    long long primaryTerm = 0;  ///< 对应 ES 的 _primary_term（>= 1 才有效）

    /**
     * 凭据是否形式上有效（可随条件写请求发送）
     */
    bool isValid() const {
        return seqNo >= 0 && primaryTerm >= 1;
    }
};

/**
 * 带编辑凭据的文档快照
 */
struct VersionedDocument {
    std::string id;
    std::string index;
    long long version = 0;      ///< 文档版本号（仅用于展示，不用于并发控制）
    EditCredential credential;  ///< 编辑凭据，条件更新/删除时原样带回
    json source;                ///< 文档内容
};

/**
 * 条件写操作的状态
 */
enum class ConditionalWriteStatus {
    Success,           ///< 凭据匹配，写入成功
    Conflict,          ///< 409：凭据已过期，文档已被他人修改
    NotFound,          ///< 404：文档不存在或已被删除
    InvalidCredential  ///< 凭据本身无效（并非读取时获得的合法凭据）
};

/**
 * 条件写操作的结果
 */
struct ConditionalWriteResult {
    ConditionalWriteStatus status = ConditionalWriteStatus::Success;
    std::string id;
    std::string index;
    std::string result;             ///< 成功时为 updated / deleted
    long long version = 0;          ///< 成功时的新版本号
    EditCredential credential;      ///< 成功时的新凭据（可凭此继续编辑）
    /// 冲突时带回的当前文档快照，供上层展示差异；
    /// 文档恰好在冲突之后被删除时为空
    std::optional<VersionedDocument> currentDoc;

    bool success() const {
        return status == ConditionalWriteStatus::Success;
    }
};

/**
 * Elasticsearch 客户端异常
 */
class ESException : public std::runtime_error {
public:
    explicit ESException(const std::string& message) 
        : std::runtime_error(message) {}
};

/**
 * Elasticsearch 客户端类
 */
class ESClient {
public:
    /**
     * 构造函数
     * @param host ES 主机地址
     * @param port ES 端口
     */
    explicit ESClient(const std::string& host = "localhost", int port = 9200);
    ~ESClient();
    
    // ==================== 集群操作 ====================
    
    /**
     * 检查 ES 连接是否正常
     */
    bool ping();
    
    /**
     * 获取集群健康状态
     */
    json clusterHealth();
    
    /**
     * 获取集群信息
     */
    json clusterInfo();
    
    // ==================== 索引操作 ====================
    
    /**
     * 创建索引
     * @param indexName 索引名称
     * @param mappings 映射配置（可选）
     * @param settings 索引设置（可选）
     */
    bool createIndex(const std::string& indexName,
                     const json& mappings = json::object(),
                     const json& settings = json::object());
    
    /**
     * 删除索引
     */
    bool deleteIndex(const std::string& indexName);
    
    /**
     * 检查索引是否存在
     */
    bool indexExists(const std::string& indexName);
    
    /**
     * 获取索引信息
     */
    json getIndex(const std::string& indexName);
    
    /**
     * 刷新索引（使文档可搜索）
     */
    bool refreshIndex(const std::string& indexName);
    
    // ==================== 文档操作 ====================
    
    /**
     * 索引文档（添加或更新）
     * @param indexName 索引名称
     * @param doc 文档内容
     * @param id 文档 ID（可选，不指定则自动生成）
     */
    DocResult indexDocument(const std::string& indexName,
                            const json& doc,
                            const std::string& id = "");
    
    /**
     * 获取文档
     */
    std::optional<json> getDocument(const std::string& indexName,
                                    const std::string& id);
    
    /**
     * 更新文档
     */
    DocResult updateDocument(const std::string& indexName,
                             const std::string& id,
                             const json& doc);
    
    /**
     * 删除文档
     */
    bool deleteDocument(const std::string& indexName,
                        const std::string& id);
    
    /**
     * 批量索引文档
     */
    BulkResult bulkIndex(const std::string& indexName,
                         const std::vector<json>& docs,
                         const std::vector<std::string>& ids = {});

    // ==================== 乐观并发控制（交互式编辑） ====================

    /**
     * 读取文档并获取编辑凭据（_seq_no / _primary_term）
     *
     * 交互式编辑的第一步：取得文档内容与编辑凭据，
     * 之后通过 updateDocumentIfMatch / deleteDocumentIfMatch 凭凭据写回。
     *
     * @return 文档存在时返回带凭据的快照，否则返回 std::nullopt
     */
    std::optional<VersionedDocument> getDocumentForEdit(const std::string& indexName,
                                                        const std::string& id);

    /**
     * 条件更新文档（乐观并发控制）
     *
     * 仅当 credential 与服务端当前 _seq_no/_primary_term 匹配时才写入；
     * 不匹配时返回 Conflict 并带回当前文档快照供上层展示差异，
     * 不会自动重试，也不会覆盖他人修改。
     *
     * 状态语义：
     * - Success：写入成功，结果携带新的版本信息与凭据
     * - Conflict：凭据已过期（409），currentDoc 为当前文档快照
     * - NotFound：文档不存在或已被删除（404）
     * - InvalidCredential：凭据本身无效（未携带读取时获得的合法凭据）
     * - 其他服务端失败（5xx 等）：抛出 ESException，与冲突明确区分
     */
    ConditionalWriteResult updateDocumentIfMatch(const std::string& indexName,
                                                 const std::string& id,
                                                 const EditCredential& credential,
                                                 const json& doc);

    /**
     * 条件删除文档（乐观并发控制）
     *
     * 状态语义与 updateDocumentIfMatch 相同。
     */
    ConditionalWriteResult deleteDocumentIfMatch(const std::string& indexName,
                                                 const std::string& id,
                                                 const EditCredential& credential);

    // ==================== 搜索操作 ====================
    
    /**
     * Match 查询（分词匹配）
     */
    SearchResult matchSearch(const std::string& indexName,
                             const std::string& field,
                             const std::string& query,
                             int from = 0,
                             int size = 10);
    
    /**
     * Multi-Match 查询（多字段匹配）
     */
    SearchResult multiMatchSearch(const std::string& indexName,
                                  const std::vector<std::string>& fields,
                                  const std::string& query,
                                  int from = 0,
                                  int size = 10);
    
    /**
     * Term 查询（精确匹配）
     */
    SearchResult termSearch(const std::string& indexName,
                            const std::string& field,
                            const std::string& value,
                            int from = 0,
                            int size = 10);
    
    /**
     * Bool 组合查询
     */
    SearchResult boolSearch(const std::string& indexName,
                            const json& must = json::array(),
                            const json& should = json::array(),
                            const json& mustNot = json::array(),
                            const json& filter = json::array(),
                            int from = 0,
                            int size = 10);
    
    /**
     * 带高亮的搜索
     */
    SearchResult searchWithHighlight(const std::string& indexName,
                                     const json& query,
                                     const std::vector<std::string>& highlightFields,
                                     int from = 0,
                                     int size = 10);
    
    /**
     * 通用搜索（自定义查询体）
     */
    SearchResult search(const std::string& indexName,
                        const json& queryBody);
    
    // ==================== 日志回调 ====================
    
    using LogCallback = std::function<void(const std::string&)>;
    
    /**
     * 设置日志回调
     */
    void setLogCallback(LogCallback callback);

private:
    std::string baseUrl_;
    HttpClient httpClient_;
    LogCallback logCallback_;
    
    void log(const std::string& message);
    std::string buildUrl(const std::string& path);
    SearchResult parseSearchResponse(const json& response);
    ConditionalWriteResult handleConditionalWriteResponse(const HttpResponse& response,
                                                          ConditionalWriteResult result);
};

} // namespace es

#endif // ES_CLIENT_HPP
