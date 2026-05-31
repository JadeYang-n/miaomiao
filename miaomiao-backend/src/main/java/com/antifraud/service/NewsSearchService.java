package com.antifraud.service;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.annotation.Resource;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.client.WebClient;

import java.time.Duration;
import java.time.Instant;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

/**
 * 新闻搜索服务 — RSS 优先 + 搜索 API 兜底
 */
@Service
public class NewsSearchService {
    private static final Logger log = LoggerFactory.getLogger(NewsSearchService.class);

    @Value("${news.search-api.url:}")
    private String searchApiUrl;

    @Value("${news.search-api.key:}")
    private String searchApiKey;

    @Resource
    private ObjectMapper objectMapper;

    // 简单缓存：keyword -> (results, expireTime)
    private final ConcurrentHashMap<String, CacheEntry> cache = new ConcurrentHashMap<>();
    private static final Duration CACHE_TTL = Duration.ofHours(1);

    /**
     * 搜索诈骗相关新闻
     */
    public List<NewsItem> searchScamNews(String keywords) {
        if (keywords == null || keywords.isEmpty()) {
            return Collections.emptyList();
        }

        // 查缓存
        CacheEntry cached = cache.get(keywords);
        if (cached != null && Instant.now().isBefore(cached.expireAt)) {
            log.info("[新闻] 缓存命中: {}", keywords);
            return cached.items;
        }

        // 搜索 API
        List<NewsItem> results = searchViaApi(keywords);

        // 写缓存
        if (!results.isEmpty()) {
            cache.put(keywords, new CacheEntry(results, Instant.now().plus(CACHE_TTL)));
        }

        return results;
    }

    /**
     * 通过搜索 API 搜索新闻
     */
    private List<NewsItem> searchViaApi(String keywords) {
        if (searchApiUrl == null || searchApiUrl.isEmpty()) {
            log.debug("[新闻] 未配置搜索 API，跳过");
            return Collections.emptyList();
        }

        try {
            WebClient client = WebClient.builder().build();

            Map<String, Object> body = Map.of(
                "query", keywords,
                "max_results", 3,
                "include_answer", false
            );

            String response = client.post()
                .uri(searchApiUrl)
                .header("Authorization", "Bearer " + searchApiKey)
                .header("Content-Type", "application/json")
                .bodyValue(body)
                .retrieve()
                .bodyToMono(String.class)
                .timeout(Duration.ofSeconds(5))
                .block();

            JsonNode json = objectMapper.readTree(response);
            List<NewsItem> items = new ArrayList<>();

            JsonNode results = json.path("results");
            if (results.isArray()) {
                for (JsonNode result : results) {
                    NewsItem item = new NewsItem();
                    item.setTitle(result.path("title").asText(""));
                    item.setSource(result.path("url").asText(""));
                    item.setDate(result.path("published_date").asText(""));
                    item.setUrl(result.path("url").asText(""));
                    if (!item.getTitle().isEmpty()) {
                        items.add(item);
                    }
                }
            }

            log.info("[新闻] 搜索 '{}' 返回 {} 条结果", keywords, items.size());
            return items;
        } catch (Exception e) {
            log.warn("[新闻] 搜索失败: {}", e.getMessage());
            return Collections.emptyList();
        }
    }

    /**
     * 将新闻列表格式化为 prompt 注入文本
     */
    public String formatNewsForPrompt(List<NewsItem> items) {
        if (items == null || items.isEmpty()) {
            return "";
        }
        StringBuilder sb = new StringBuilder("\n【相关新闻佐证】\n");
        for (NewsItem item : items) {
            sb.append("- ").append(item.getTitle());
            if (item.getSource() != null && !item.getSource().isEmpty()) {
                sb.append(" (").append(item.getSource()).append(")");
            }
            if (item.getDate() != null && !item.getDate().isEmpty()) {
                sb.append(" ").append(item.getDate());
            }
            sb.append("\n");
        }
        return sb.toString();
    }

    public static class NewsItem {
        private String title;
        private String source;
        private String date;
        private String url;

        public String getTitle() { return title; }
        public void setTitle(String title) { this.title = title; }
        public String getSource() { return source; }
        public void setSource(String source) { this.source = source; }
        public String getDate() { return date; }
        public void setDate(String date) { this.date = date; }
        public String getUrl() { return url; }
        public void setUrl(String url) { this.url = url; }
    }

    private static class CacheEntry {
        final List<NewsItem> items;
        final Instant expireAt;
        CacheEntry(List<NewsItem> items, Instant expireAt) {
            this.items = items;
            this.expireAt = expireAt;
        }
    }
}
