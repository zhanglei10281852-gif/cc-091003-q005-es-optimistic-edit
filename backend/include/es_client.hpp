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
 * 文档版本凭据（乐观并发控制的"编辑凭据"）
 *
 * 通过 getDocumentForUpdate() 读取文档时一并取得；
 * 之后的条件更新/条件删除必须原样携带这对凭据，
 * 仅当服务器上文档的当前凭据与之完全一致时写入才会生效。
 */
struct VersionInfo {
    long long seqNo = -1;       ///< _seq_no：分片级序列号，每次写入递增
    long long primaryTerm = -1; ///< _primary_term：主分片任期
    int version = 0;            ///< _version：文档版本号（仅供展示）

    /**
     * 凭据是否可用于条件写入（seq_no >= 0 且 primary_term >= 1）
     */
    bool valid() const { return seqNo >= 0 && primaryTerm >= 1; }
};

/**
 * 带编辑凭据的文档快照
 */
struct VersionedDocument {
    std::string id;
    std::string index;
    VersionInfo version;  ///< 编辑凭据：条件更新/删除时原样带回
    json source;          ///< 文档内容（_source）
};

/**
 * 条件写入结果状态
 */
enum class ConditionalStatus {
    Success,   ///< 凭据匹配，写入成功
    Conflict,  ///< 409：凭据已过期，文档已被他人修改或删除
    NotFound   ///< 文档不存在（从未创建，或删除标记已被清理）
};

/**
 * 条件写入状态的可读描述
 */
const char* toString(ConditionalStatus status);

/**
 * 条件写入结果
 */
struct ConditionalWriteResult {
    ConditionalStatus status = ConditionalStatus::Conflict;
    std::string result;                       ///< 成功时 ES 返回的操作结果：updated / deleted
    VersionInfo newVersion;                   ///< 成功时：写入后的新编辑凭据
    std::optional<VersionedDocument> current; ///< 冲突时：服务器当前快照；文档已被删除时为 nullopt
    std::string errorType;                    ///< 失败时服务端返回的 error.type

    bool success() const { return status == ConditionalStatus::Success; }
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

    // ==================== 乐观并发控制（条件读写） ====================

    /**
     * 读取文档并获取编辑凭据（_seq_no + _primary_term）
     *
     * 交互式编辑场景应使用本方法打开文档，
     * 随后将返回快照中的 version 作为条件更新/删除的凭据。
     * @return 文档快照；文档不存在或已删除时返回 std::nullopt
     */
    std::optional<VersionedDocument> getDocumentForUpdate(const std::string& indexName,
                                                          const std::string& id);

    /**
     * 条件更新：仅当文档当前的 _seq_no/_primary_term 与 expected 一致时才写入。
     *
     * - 凭据匹配：返回 Success，newVersion 为写入后的新编辑凭据
     * - 凭据过期（409）：返回 Conflict，并带回服务器当前快照（current）供上层
     *   展示差异；文档已被他人删除时 current 为 std::nullopt。
     *   不会自动重试，也不会覆盖他人修改
     * - 文档不存在：返回 NotFound
     * - 凭据无效（如 seq_no 为负）：抛出 ESException（客户端校验，不发送请求）
     * - 其他服务端错误（5xx、400 等）：抛出 ESException
     */
    ConditionalWriteResult updateDocument(const std::string& indexName,
                                          const std::string& id,
                                          const json& doc,
                                          const VersionInfo& expected);

    /**
     * 条件删除：仅当文档当前的 _seq_no/_primary_term 与 expected 一致时才删除。
     * 结果状态语义同条件更新。
     */
    ConditionalWriteResult deleteDocument(const std::string& indexName,
                                          const std::string& id,
                                          const VersionInfo& expected);
    
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
    ConditionalWriteResult parseConditionalResponse(const HttpResponse& response,
                                                    const std::string& indexName,
                                                    const std::string& id);
};

} // namespace es

#endif // ES_CLIENT_HPP
