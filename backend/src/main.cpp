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

// ==================== 乐观并发演示辅助 ====================

std::string formatCredential(const VersionInfo& v) {
    return "seq_no=" + std::to_string(v.seqNo) +
           ", primary_term=" + std::to_string(v.primaryTerm) +
           " (_version=" + std::to_string(v.version) + ")";
}

void printArticleBrief(const json& source) {
    std::cout << "      title: " << source.value("title", "") << "\n";
    std::cout << "      tags:  " << source.value("tags", json::array()).dump() << "\n";
}

/**
 * 乐观并发控制演示：
 * 两个独立的 ESClient 实例模拟两名编辑同时编辑同一篇文章。
 * 场景一：交错保存 —— 首个修改成功、旧凭据被拒、重新读取并人工合并后成功；
 * 场景二：保存后再删除 —— 过期凭据的删除/保存均被拒，文章保持已确认的状态。
 */
void demoOptimisticConcurrency(const std::string& host, int port) {
    printSection(10, "乐观并发控制演示（两名编辑同时编辑同一篇文章）");

    const std::string indexName = "editor_articles";
    const std::string docId = "article-1";
    bool allOk = true;

    // 校验辅助：打印结果并记录失败
    auto check = [&allOk](bool ok, const std::string& label) {
        if (ok) {
            printSuccess(label);
        } else {
            printError(label + " 【校验失败】");
            allOk = false;
        }
    };

    // 两个独立的客户端连接 = 两名编辑各自独立的编辑会话
    ESClient editorA(host, port);
    ESClient editorB(host, port);

    try {
        // ---------- 准备：非交互式导入（仍使用原有的无条件写入接口） ----------
        if (editorA.indexExists(indexName)) {
            editorA.deleteIndex(indexName);
        }
        json mappings = {
            {"properties", {
                {"title", {{"type", "text"}, {"analyzer", "standard"}}},
                {"content", {{"type", "text"}, {"analyzer", "standard"}}},
                {"tags", {{"type", "keyword"}}}
            }}
        };
        editorA.createIndex(indexName, mappings);

        json article = {
            {"title", "Elasticsearch 实战指南"},
            {"content", "本文介绍 Elasticsearch 的核心概念与使用方法。"},
            {"tags", json::array({"Elasticsearch", "搜索"})}
        };
        auto imported = editorA.indexDocument(indexName, article, docId);  // 无条件写入
        editorA.refreshIndex(indexName);
        printInfo("初始文章已通过无条件接口导入, ID: " + imported.id);

        // ==================== 场景一：交错保存 ====================
        printInfo("场景一：两名编辑交错保存同一篇文章");

        // 1) 两名编辑同时打开文章，拿到相同的编辑凭据
        auto docA = editorA.getDocumentForUpdate(indexName, docId);
        auto docB = editorB.getDocumentForUpdate(indexName, docId);
        if (!docA || !docB) {
            throw ESException("演示数据异常：文章读取失败");
        }
        std::cout << "  编辑A 打开文章，凭据: " << formatCredential(docA->version) << "\n";
        std::cout << "  编辑B 打开文章，凭据: " << formatCredential(docB->version) << "\n";
        check(docA->version.seqNo == docB->version.seqNo &&
              docA->version.primaryTerm == docB->version.primaryTerm,
              "两名编辑拿到相同凭据（基于同一版本编辑）");

        // 2) 编辑A 先保存（修改标题）→ 成功并获得新凭据
        auto saveA = editorA.updateDocument(indexName, docId,
            {{"title", "Elasticsearch 实战指南（2024 修订版）"}}, docA->version);
        check(saveA.success(), "编辑A 保存成功（首个修改被接受）");
        std::cout << "  编辑A 新凭据: " << formatCredential(saveA.newVersion) << "\n";

        // 3) 编辑B 仍持旧凭据保存（修改标签）→ 409 冲突，客户端带回当前快照
        json editBTags = json::array({"Elasticsearch", "搜索", "Kibana"});
        auto saveB = editorB.updateDocument(indexName, docId,
            {{"tags", editBTags}}, docB->version);
        check(saveB.status == ConditionalStatus::Conflict,
              std::string("编辑B 的旧凭据被拒绝（status=") + toString(saveB.status) +
              (saveB.errorType.empty() ? "" : ", " + saveB.errorType) + "）");
        if (saveB.current) {
            std::cout << Color::YELLOW << "  冲突！可据此向编辑展示差异：\n" << Color::RESET;
            std::cout << "  服务器当前快照 (凭据: " << formatCredential(saveB.current->version) << "):\n";
            printArticleBrief(saveB.current->source);
            std::cout << "  编辑B 本地待保存的修改:\n";
            std::cout << "      tags:  " << editBTags.dump() << "\n";
        }
        check(saveB.current.has_value() &&
              saveB.current->source.value("title", "") == "Elasticsearch 实战指南（2024 修订版）",
              "冲突响应带回了编辑A 保存后的当前快照");

        // 4) 编辑B 重新读取 → 人工合并 → 携带新凭据再次保存 → 成功
        auto fresh = editorB.getDocumentForUpdate(indexName, docId);
        if (!fresh) {
            throw ESException("演示数据异常：文章重新读取失败");
        }
        json merged = fresh->source;          // 保留服务器上编辑A 的标题
        merged["tags"] = editBTags;           // 合并编辑B 自己的标签修改
        std::cout << "  编辑B 重新读取 (凭据: " << formatCredential(fresh->version)
                  << ")，人工合并后重新保存...\n";
        auto saveB2 = editorB.updateDocument(indexName, docId, merged, fresh->version);
        check(saveB2.success(), "编辑B 合并后保存成功");

        // 5) 校验最终内容：两名编辑的修改都保留，没有静默覆盖
        auto finalDoc = editorA.getDocumentForUpdate(indexName, docId);
        check(finalDoc.has_value() &&
              finalDoc->source.value("title", "") == "Elasticsearch 实战指南（2024 修订版）" &&
              finalDoc->source.value("tags", json::array()) == editBTags,
              "最终内容 = 编辑A 的标题 + 编辑B 的标签（双方修改均保留）");
        if (finalDoc) {
            printArticleBrief(finalDoc->source);
        }

        // ==================== 场景二：保存后再删除 ====================
        printInfo("场景二：保存后再删除 —— 过期凭据的删除与迟到保存");

        // 1) 编辑A 打开文章（拿到当前凭据）
        auto opened = editorA.getDocumentForUpdate(indexName, docId);
        if (!opened) {
            throw ESException("演示数据异常：文章读取失败");
        }
        std::cout << "  编辑A 打开文章，凭据: " << formatCredential(opened->version) << "\n";

        // 2) 编辑B 仍持有场景一的旧凭据，尝试直接删除 → 被拒绝
        auto staleDelete = editorB.deleteDocument(indexName, docId, docB->version);
        check(staleDelete.status == ConditionalStatus::Conflict &&
              staleDelete.current.has_value(),
              "编辑B 用过期凭据删除被拒绝（文章未被误删）");

        // 3) 编辑B 重新读取，凭最新凭据删除 → 成功
        auto beforeDelete = editorB.getDocumentForUpdate(indexName, docId);
        if (!beforeDelete) {
            throw ESException("演示数据异常：文章读取失败");
        }
        auto del = editorB.deleteDocument(indexName, docId, beforeDelete->version);
        check(del.success() && del.result == "deleted",
              "编辑B 凭最新凭据删除成功（删除被明确确认）");

        // 4) 编辑A 仍持旧凭据保存 → 冲突，当前快照为空 = 文章已被删除
        auto lateSave = editorA.updateDocument(indexName, docId,
            {{"title", "编辑A 的迟到保存"}}, opened->version);
        if (lateSave.status == ConditionalStatus::Conflict && !lateSave.current) {
            printSuccess("编辑A 的迟到保存被拒绝：文章已被他人删除"
                         "（冲突且无当前快照），未自动重建");
        } else if (lateSave.status == ConditionalStatus::NotFound) {
            printSuccess("编辑A 的迟到保存被拒绝：文章已不存在"
                         "（not_found），未自动重建");
        } else {
            printError("编辑A 的迟到保存未被正确拒绝 【校验失败】");
            allOk = false;
        }

        // 5) 校验：文章确实保持已删除状态 —— ES 中只保留明确确认过的内容
        auto gone = editorA.getDocumentForUpdate(indexName, docId);
        check(!gone.has_value(), "最终状态：文章保持已删除，未被旧凭据的写入复活");

        // ==================== 边界：与 409 冲突区分的其他结果 ====================
        printInfo("边界情况：区分冲突、文档不存在与无效凭据");

        // 对不存在的文档做条件更新 → NotFound（不是冲突）
        auto missing = editorA.updateDocument(indexName, "no-such-article",
            {{"title", "不存在的文章"}}, VersionInfo{0, 1});
        check(missing.status == ConditionalStatus::NotFound,
              std::string("条件更新不存在的文档 → ") + toString(missing.status) +
              "（与 409 冲突区分）");

        // 无效凭据（seq_no 为负）→ 客户端校验直接拒绝，不发送请求
        try {
            (void)editorA.updateDocument(indexName, docId,
                {{"title", "x"}}, VersionInfo{-5, 1});
            printError("无效凭据未被拒绝 【校验失败】");
            allOk = false;
        } catch (const ESException& e) {
            printSuccess(std::string("无效凭据被客户端拒绝: ") + e.what());
        }

        // ---------- 清理演示索引 ----------
        editorA.deleteIndex(indexName);
        printInfo("演示索引 '" + indexName + "' 已清理");

        if (!allOk) {
            throw ESException("乐观并发演示存在校验失败项");
        }
        printSuccess("乐观并发控制演示全部通过");
    } catch (...) {
        // 尽量清理演示索引，避免残留
        try { editorA.deleteIndex(indexName); } catch (...) {}
        throw;
    }
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
        demoOptimisticConcurrency(host, port);
        demoCleanup(client, indexName);
        
        printHeader("演示完成！");
        
    } catch (const std::exception& e) {
        printError(std::string("程序异常: ") + e.what());
        return 1;
    }
    
    return 0;
}
