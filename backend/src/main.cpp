#include "es_client.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>

using namespace es;
using json = nlohmann::json;

// ==================== 控制台颜色 ====================

namespace Color {
    const std::string RESET   = "\033[0m";
    const std::string RED     = "\033[31m";
    const std::string GREEN   = "\033[32m";
    const std::string YELLOW  = "\033[33m";
    const std::string BLUE    = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN    = "\033[36m";
    const std::string BOLD    = "\033[1m";
}

void printHeader(const std::string& title) {
    std::cout << "\n" << Color::CYAN << Color::BOLD;
    std::cout << "========================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "========================================\n";
    std::cout << Color::RESET;
}

void printSection(int num, const std::string& title) {
    std::cout << "\n" << Color::YELLOW << Color::BOLD;
    std::cout << "[" << num << "] " << title << "\n";
    std::cout << Color::RESET;
}

void printSuccess(const std::string& message) {
    std::cout << Color::GREEN << "✓ " << message << Color::RESET << "\n";
}

void printError(const std::string& message) {
    std::cout << Color::RED << "✗ " << message << Color::RESET << "\n";
}

void printInfo(const std::string& message) {
    std::cout << Color::BLUE << "→ " << message << Color::RESET << "\n";
}

// ==================== 示例数据 ====================

std::vector<json> getSampleArticles() {
    return {
        {
            {"title", "人工智能的发展历程"},
            {"content", "人工智能（AI）是计算机科学的一个分支，致力于创建能够执行通常需要人类智能的任务的系统。从1956年达特茅斯会议开始，AI经历了多次发展浪潮。"},
            {"author", "张三"},
            {"category", "技术"},
            {"tags", json::array({"AI", "人工智能", "机器学习"})},
            {"created_at", "2024-01-15"}
        },
        {
            {"title", "深度学习入门指南"},
            {"content", "深度学习是机器学习的一个子领域，使用多层神经网络来学习数据的层次化表示。本文将介绍深度学习的基本概念和常用框架。"},
            {"author", "李四"},
            {"category", "技术"},
            {"tags", json::array({"深度学习", "神经网络", "TensorFlow"})},
            {"created_at", "2024-02-20"}
        },
        {
            {"title", "Elasticsearch 搜索引擎实战"},
            {"content", "Elasticsearch是一个分布式、RESTful风格的搜索和数据分析引擎。它能够快速地存储、搜索和分析大量数据，广泛应用于日志分析、全文搜索等场景。"},
            {"author", "王五"},
            {"category", "技术"},
            {"tags", json::array({"Elasticsearch", "搜索引擎", "全文检索"})},
            {"created_at", "2024-03-10"}
        },
        {
            {"title", "C++17 新特性详解"},
            {"content", "C++17引入了许多新特性，包括结构化绑定、if constexpr、折叠表达式等。这些特性使得C++代码更加简洁和高效。"},
            {"author", "赵六"},
            {"category", "编程语言"},
            {"tags", json::array({"C++", "C++17", "编程"})},
            {"created_at", "2024-04-05"}
        },
        {
            {"title", "微服务架构设计模式"},
            {"content", "微服务架构是一种将应用程序构建为一组小型服务的方法。每个服务运行在自己的进程中，通过轻量级机制（通常是HTTP API）进行通信。"},
            {"author", "钱七"},
            {"category", "架构"},
            {"tags", json::array({"微服务", "架构", "分布式"})},
            {"created_at", "2024-05-18"}
        }
    };
}

// ==================== 演示函数 ====================

void demoClusterInfo(ESClient& client) {
    printSection(1, "获取集群信息");
    
    try {
        auto info = client.clusterInfo();
        std::cout << "  集群名称: " << info["cluster_name"] << "\n";
        std::cout << "  ES 版本: " << info["version"]["number"] << "\n";
        
        auto health = client.clusterHealth();
        std::cout << "  集群状态: " << health["status"] << "\n";
        std::cout << "  节点数量: " << health["number_of_nodes"] << "\n";
        
        printSuccess("集群连接正常");
    } catch (const std::exception& e) {
        printError(std::string("获取集群信息失败: ") + e.what());
        throw;
    }
}

void demoCreateIndex(ESClient& client, const std::string& indexName) {
    printSection(2, "创建索引 '" + indexName + "'");
    
    // 如果索引已存在，先删除
    if (client.indexExists(indexName)) {
        printInfo("索引已存在，先删除...");
        client.deleteIndex(indexName);
    }
    
    // 定义索引映射
    // 注：使用 standard 分词器，对中文按单字切分
    // 如需真正的中文分词，需安装 IK 分词器插件，并将 analyzer 改为 "ik_max_word" 或 "ik_smart"
    json mappings = {
        {"properties", {
            {"title", {
                {"type", "text"},
                {"analyzer", "standard"},
                {"fields", {
                    {"keyword", {{"type", "keyword"}}}
                }}
            }},
            {"content", {
                {"type", "text"},
                {"analyzer", "standard"}
            }},
            {"author", {
                {"type", "keyword"}
            }},
            {"category", {
                {"type", "keyword"}
            }},
            {"tags", {
                {"type", "keyword"}
            }},
            {"created_at", {
                {"type", "date"},
                {"format", "yyyy-MM-dd"}
            }}
        }}
    };
    
    // 索引设置
    json settings = {
        {"number_of_shards", 1},
        {"number_of_replicas", 0}
    };
    
    try {
        client.createIndex(indexName, mappings, settings);
        printSuccess("索引创建成功");
    } catch (const std::exception& e) {
        printError(std::string("索引创建失败: ") + e.what());
        throw;
    }
}

void demoBulkIndex(ESClient& client, const std::string& indexName) {
    printSection(3, "批量导入文档");
    
    auto articles = getSampleArticles();
    std::vector<std::string> ids = {"1", "2", "3", "4", "5"};
    
    try {
        auto result = client.bulkIndex(indexName, articles, ids);
        printSuccess("成功导入 " + std::to_string(result.successCount) + " 篇文章");
        
        if (result.failCount > 0) {
            printError("失败 " + std::to_string(result.failCount) + " 篇");
        }
        
        // 刷新索引使文档可搜索
        client.refreshIndex(indexName);
        printInfo("索引已刷新，文档可搜索");
    } catch (const std::exception& e) {
        printError(std::string("批量导入失败: ") + e.what());
        throw;
    }
}

void demoMatchSearch(ESClient& client, const std::string& indexName) {
    printSection(4, "Match 查询演示");
    
    std::string keyword = "人工智能";
    printInfo("搜索关键词: \"" + keyword + "\"");
    
    try {
        auto result = client.matchSearch(indexName, "content", keyword);
        
        std::cout << "\n  命中 " << Color::BOLD << result.total << Color::RESET 
                  << " 条结果 (耗时 " << result.took << "ms)\n\n";
        
        for (size_t i = 0; i < result.hits.size(); ++i) {
            const auto& hit = result.hits[i];
            std::cout << "  [" << (i + 1) << "] " 
                      << Color::BOLD << hit.source["title"].get<std::string>() << Color::RESET
                      << " (score: " << std::fixed << std::setprecision(2) << hit.score << ")\n";
            std::cout << "      作者: " << hit.source["author"].get<std::string>() 
                      << " | 分类: " << hit.source["category"].get<std::string>() << "\n";
        }
    } catch (const std::exception& e) {
        printError(std::string("搜索失败: ") + e.what());
    }
}

void demoMultiMatchSearch(ESClient& client, const std::string& indexName) {
    printSection(5, "Multi-Match 查询演示（多字段搜索）");
    
    std::string keyword = "深度学习";
    std::vector<std::string> fields = {"title", "content"};
    printInfo("搜索关键词: \"" + keyword + "\" (在 title 和 content 中)");
    
    try {
        auto result = client.multiMatchSearch(indexName, fields, keyword);
        
        std::cout << "\n  命中 " << Color::BOLD << result.total << Color::RESET 
                  << " 条结果\n\n";
        
        for (const auto& hit : result.hits) {
            std::cout << "  • " << Color::BOLD << hit.source["title"].get<std::string>() 
                      << Color::RESET << "\n";
            
            // 截取内容前50个字符
            std::string content = hit.source["content"].get<std::string>();
            if (content.length() > 80) {
                content = content.substr(0, 80) + "...";
            }
            std::cout << "    " << Color::CYAN << content << Color::RESET << "\n\n";
        }
    } catch (const std::exception& e) {
        printError(std::string("搜索失败: ") + e.what());
    }
}

void demoTermSearch(ESClient& client, const std::string& indexName) {
    printSection(6, "Term 查询演示（精确匹配）");
    
    std::string author = "王五";
    printInfo("精确匹配作者: \"" + author + "\"");
    
    try {
        auto result = client.termSearch(indexName, "author", author);
        
        std::cout << "\n  命中 " << Color::BOLD << result.total << Color::RESET 
                  << " 条结果\n\n";
        
        for (const auto& hit : result.hits) {
            std::cout << "  • " << hit.source["title"].get<std::string>() << "\n";
            std::cout << "    作者: " << hit.source["author"].get<std::string>() << "\n";
        }
    } catch (const std::exception& e) {
        printError(std::string("搜索失败: ") + e.what());
    }
}

void demoBoolSearch(ESClient& client, const std::string& indexName) {
    printSection(7, "Bool 组合查询演示");
    
    printInfo("查询条件: 分类='技术' AND 内容包含'学习'");
    
    try {
        json must = json::array({
            {{"match", {{"content", "学习"}}}}
        });
        
        json filter = json::array({
            {{"term", {{"category", "技术"}}}}
        });
        
        auto result = client.boolSearch(indexName, must, json::array(), json::array(), filter);
        
        std::cout << "\n  命中 " << Color::BOLD << result.total << Color::RESET 
                  << " 条结果\n\n";
        
        for (const auto& hit : result.hits) {
            std::cout << "  • " << hit.source["title"].get<std::string>() << "\n";
            std::cout << "    分类: " << hit.source["category"].get<std::string>() 
                      << " | Score: " << std::fixed << std::setprecision(2) << hit.score << "\n";
        }
    } catch (const std::exception& e) {
        printError(std::string("搜索失败: ") + e.what());
    }
}

void demoHighlightSearch(ESClient& client, const std::string& indexName) {
    printSection(8, "高亮搜索演示");
    
    std::string keyword = "Elasticsearch";
    printInfo("搜索关键词: \"" + keyword + "\" (带高亮)");
    
    try {
        json query = {
            {"multi_match", {
                {"query", keyword},
                {"fields", json::array({"title", "content"})}
            }}
        };
        
        auto result = client.searchWithHighlight(indexName, query, {"title", "content"});
        
        std::cout << "\n  命中 " << Color::BOLD << result.total << Color::RESET 
                  << " 条结果\n\n";
        
        for (const auto& hit : result.hits) {
            std::cout << "  标题: " << hit.source["title"].get<std::string>() << "\n";
            
            // 显示高亮内容
            if (hit.highlight.contains("content")) {
                std::cout << "  高亮: ";
                for (const auto& fragment : hit.highlight["content"]) {
                    // 将 <em> 标签替换为颜色
                    std::string text = fragment.get<std::string>();
                    size_t pos = 0;
                    while ((pos = text.find("<em>", pos)) != std::string::npos) {
                        text.replace(pos, 4, Color::RED + Color::BOLD);
                        pos += Color::RED.length() + Color::BOLD.length();
                    }
                    pos = 0;
                    while ((pos = text.find("</em>", pos)) != std::string::npos) {
                        text.replace(pos, 5, Color::RESET);
                        pos += Color::RESET.length();
                    }
                    std::cout << text << "\n";
                }
            }
            std::cout << "\n";
        }
    } catch (const std::exception& e) {
        printError(std::string("搜索失败: ") + e.what());
    }
}

void demoDocumentCRUD(ESClient& client, const std::string& indexName) {
    printSection(9, "文档 CRUD 操作演示");
    
    // 创建文档
    printInfo("创建新文档...");
    json newDoc = {
        {"title", "测试文档"},
        {"content", "这是一个用于演示CRUD操作的测试文档"},
        {"author", "测试用户"},
        {"category", "测试"},
        {"tags", json::array({"test", "demo"})},
        {"created_at", "2024-06-01"}
    };
    
    try {
        auto createResult = client.indexDocument(indexName, newDoc, "test-doc-1");
        printSuccess("文档创建成功, ID: " + createResult.id);
        
        // 刷新使文档可搜索
        client.refreshIndex(indexName);
        
        // 读取文档
        printInfo("读取文档...");
        auto doc = client.getDocument(indexName, "test-doc-1");
        if (doc) {
            printSuccess("文档读取成功: " + (*doc)["title"].get<std::string>());
        }
        
        // 更新文档
        printInfo("更新文档...");
        json updateData = {{"title", "更新后的测试文档"}};
        auto updateResult = client.updateDocument(indexName, "test-doc-1", updateData);
        printSuccess("文档更新成功, 版本: " + std::to_string(updateResult.version));
        
        // 删除文档
        printInfo("删除文档...");
        bool deleted = client.deleteDocument(indexName, "test-doc-1");
        if (deleted) {
            printSuccess("文档删除成功");
        }
    } catch (const std::exception& e) {
        printError(std::string("CRUD 操作失败: ") + e.what());
    }
}

// 乐观并发控制演示：两个独立客户端交错编辑同一篇文章
// 返回 true 表示所有检查通过
bool demoOptimisticConcurrency(ESClient& client,
                               const std::string& host,
                               int port,
                               const std::string& indexName) {
    printSection(10, "乐观并发控制演示（编辑冲突防护）");

    bool allOk = true;
    auto check = [&](bool ok, const std::string& message) {
        if (ok) {
            printSuccess(message);
        } else {
            printError(message);
            allOk = false;
        }
    };

    // 两个相互独立的客户端，模拟两位编辑同时工作
    ESClient editorA(host, port);
    ESClient editorB(host, port);
    const std::string docId = "concurrency-demo-1";

    // 准备：非交互式导入仍使用原有的无条件写入接口
    printInfo("导入演示文章（无条件写入接口，模拟非交互式导入）...");
    json article = {
        {"title", "并发编辑演示"},
        {"content", "两位编辑同时打开这篇文章，分别修改标题和标签。"},
        {"author", "编辑部"},
        {"category", "技术"},
        {"tags", json::array({"初始标签"})},
        {"created_at", "2024-07-01"}
    };
    client.indexDocument(indexName, article, docId);

    // ==================== 场景一：交错保存 ====================
    printInfo("场景一：两位编辑同时打开同一篇文章，交错保存");

    // 1. 两位编辑同时读取文章，各自取得编辑凭据
    auto docA = editorA.getDocumentForEdit(indexName, docId);
    auto docB = editorB.getDocumentForEdit(indexName, docId);
    check(docA.has_value() && docB.has_value(), "两位编辑均读取到文章并取得编辑凭据");
    if (!docA || !docB) return false;
    std::cout << "  编辑A 凭据: seq_no=" << docA->credential.seqNo
              << ", primary_term=" << docA->credential.primaryTerm << "\n";
    std::cout << "  编辑B 凭据: seq_no=" << docB->credential.seqNo
              << ", primary_term=" << docB->credential.primaryTerm << "\n";

    // 2. 编辑A 先保存：修改标题和标签（首个修改成功）
    printInfo("编辑A 修改标题并保存...");
    json saveA = {
        {"title", "并发编辑演示（编辑A修订）"},
        {"tags", json::array({"初始标签", "A的批注"})}
    };
    auto resA = editorA.updateDocumentIfMatch(indexName, docId, docA->credential, saveA);
    check(resA.success(), "编辑A 首个保存成功，获得新凭据 seq_no=" +
                          std::to_string(resA.credential.seqNo));

    // 3. 编辑B 持旧凭据保存：应被拒绝（409 冲突），并带回当前快照
    printInfo("编辑B 持旧凭据保存自己的修改...");
    json saveB = {
        {"title", "并发编辑演示（编辑B起的标题）"},
        {"tags", json::array({"初始标签", "B的分类"})}
    };
    auto resB = editorB.updateDocumentIfMatch(indexName, docId, docB->credential, saveB);
    check(resB.status == ConditionalWriteStatus::Conflict,
          "编辑B 的旧凭据被拒绝（409 冲突），未静默覆盖编辑A 的修改");
    check(resB.currentDoc.has_value(), "冲突结果带回当前文章快照，供上层展示差异");

    if (resB.currentDoc) {
        // 上层展示差异：编辑B 的本地修改 vs 服务端当前内容
        std::cout << "  --- 差异展示（编辑B 视角）---\n";
        std::cout << "  本地标题: " << saveB["title"].get<std::string>() << "\n";
        std::cout << "  当前标题: " << resB.currentDoc->source["title"].get<std::string>() << "\n";
        std::cout << "  本地标签: " << saveB["tags"].dump() << "\n";
        std::cout << "  当前标签: " << resB.currentDoc->source["tags"].dump() << "\n";
    }

    // 4. 编辑B 重新读取，人工合并后再次保存
    printInfo("编辑B 重新读取文章，人工合并后再次保存...");
    auto freshB = editorB.getDocumentForEdit(indexName, docId);
    check(freshB.has_value(), "编辑B 重新读取成功，取得最新凭据");
    if (!freshB) return false;
    json merged = {
        {"title", freshB->source["title"]},                      // 保留编辑A 的标题
        {"tags", json::array({"初始标签", "A的批注", "B的分类"})}  // 人工合并双方标签
    };
    auto resB2 = editorB.updateDocumentIfMatch(indexName, docId, freshB->credential, merged);
    check(resB2.success(), "编辑B 人工合并后保存成功");

    // 5. 最终确认：ES 中只保留双方明确确认过的内容
    auto finalDoc = editorA.getDocumentForEdit(indexName, docId);
    check(finalDoc.has_value(), "最终读取文章成功");
    if (finalDoc) {
        bool contentOk = finalDoc->source["title"] == "并发编辑演示（编辑A修订）" &&
                         finalDoc->source["tags"] == json::array({"初始标签", "A的批注", "B的分类"});
        check(contentOk, "最终内容为双方明确确认过的合并结果");
        std::cout << "  最终标题: " << finalDoc->source["title"].get<std::string>() << "\n";
        std::cout << "  最终标签: " << finalDoc->source["tags"].dump() << "\n";
    }

    // ==================== 场景二：保存后再删除 ====================
    printInfo("场景二：一位编辑保存后，另一位编辑删除文章");

    // 1. 两位编辑分别读取当前文章（凭据一致）
    auto docA2 = editorA.getDocumentForEdit(indexName, docId);
    auto docB2 = editorB.getDocumentForEdit(indexName, docId);
    check(docA2.has_value() && docB2.has_value(), "两位编辑均读取到最新文章");
    if (!docA2 || !docB2) return false;

    // 2. 编辑A 先保存一处修改（首个修改成功）
    printInfo("编辑A 再次修改并保存...");
    json saveA2 = {
        {"tags", json::array({"初始标签", "A的批注", "B的分类", "A的终稿"})}
    };
    auto resA2 = editorA.updateDocumentIfMatch(indexName, docId, docA2->credential, saveA2);
    check(resA2.success(), "编辑A 保存成功");

    // 3. 编辑B 持旧凭据尝试删除：删除同样需要凭据，应被拒绝（409 冲突）
    printInfo("编辑B 持旧凭据尝试删除文章...");
    auto delB = editorB.deleteDocumentIfMatch(indexName, docId, docB2->credential);
    check(delB.status == ConditionalWriteStatus::Conflict,
          "编辑B 的旧凭据删除被拒绝（409 冲突）");
    check(delB.currentDoc.has_value(), "删除冲突同样带回当前快照");

    // 4. 编辑B 重新读取，确认内容后删除
    printInfo("编辑B 重新读取并确认后删除...");
    auto freshB2 = editorB.getDocumentForEdit(indexName, docId);
    check(freshB2.has_value(), "编辑B 重新读取成功");
    if (!freshB2) return false;
    auto delB2 = editorB.deleteDocumentIfMatch(indexName, docId, freshB2->credential);
    check(delB2.success(), "编辑B 凭最新凭据删除成功");

    // 5. 编辑A 持已失效凭据再保存：文档已删除，返回 NotFound（与 409 冲突区分）
    printInfo("编辑A 持失效凭据再次保存...");
    auto resA3 = editorA.updateDocumentIfMatch(indexName, docId, resA2.credential,
                                               json{{"title", "不应出现的标题"}});
    check(resA3.status == ConditionalWriteStatus::NotFound,
          "文档已删除，返回 NotFound 而非冲突");

    // 6. 最终确认：文章已在明确确认后删除
    auto gone = editorA.getDocumentForEdit(indexName, docId);
    check(!gone.has_value(), "最终确认：文章已删除，ES 中只保留明确确认过的内容");

    // ==================== 场景三：无效凭据识别 ====================
    printInfo("场景三：无效凭据识别");
    EditCredential badCredential;  // 默认构造（seq_no=-1, primary_term=0），并非读取获得
    auto badResult = editorA.updateDocumentIfMatch(indexName, docId, badCredential,
                                                   json{{"title", "不应出现的标题"}});
    check(badResult.status == ConditionalWriteStatus::InvalidCredential,
          "无效凭据被识别为 InvalidCredential，与冲突及服务端失败区分");

    return allOk;
}

void demoCleanup(ESClient& client, const std::string& indexName) {
    printSection(11, "清理资源");
    
    try {
        client.deleteIndex(indexName);
        printSuccess("索引 '" + indexName + "' 已删除");
    } catch (const std::exception& e) {
        printError(std::string("清理失败: ") + e.what());
    }
}

// ==================== 主函数 ====================

int main() {
    // 从环境变量获取 ES 配置
    const char* esHost = std::getenv("ES_HOST");
    const char* esPort = std::getenv("ES_PORT");
    
    std::string host = esHost ? esHost : "localhost";
    int port = esPort ? std::stoi(esPort) : 9200;
    
    printHeader("Elasticsearch C++ 全文检索 DEMO");
    
    std::cout << "\n连接到 Elasticsearch: " << host << ":" << port << "\n";
    
    try {
        // 创建 ES 客户端
        ESClient client(host, port);
        
        // 设置日志回调（可选）
        client.setLogCallback([](const std::string& msg) {
            // std::cout << Color::MAGENTA << "[LOG] " << msg << Color::RESET << "\n";
        });
        
        // 等待 ES 就绪
        std::cout << "等待 Elasticsearch 就绪";
        int retries = 30;
        while (!client.ping() && retries > 0) {
            std::cout << "." << std::flush;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            retries--;
        }
        std::cout << "\n";
        
        if (retries == 0) {
            printError("无法连接到 Elasticsearch，请确保服务已启动");
            return 1;
        }
        
        const std::string indexName = "articles";
        
        // 执行演示
        demoClusterInfo(client);
        demoCreateIndex(client, indexName);
        demoBulkIndex(client, indexName);
        
        // 等待索引刷新
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        demoMatchSearch(client, indexName);
        demoMultiMatchSearch(client, indexName);
        demoTermSearch(client, indexName);
        demoBoolSearch(client, indexName);
        demoHighlightSearch(client, indexName);
        demoDocumentCRUD(client, indexName);
        bool concurrencyOk = demoOptimisticConcurrency(client, host, port, indexName);
        demoCleanup(client, indexName);

        if (!concurrencyOk) {
            printError("乐观并发控制演示存在未通过的检查");
            return 1;
        }

        printHeader("演示完成！");
        
    } catch (const std::exception& e) {
        printError(std::string("程序异常: ") + e.what());
        return 1;
    }
    
    return 0;
}
