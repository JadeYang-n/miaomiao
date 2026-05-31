package com.antifraud.entity;

import java.time.LocalDateTime;

/**
 * 聊天消息实体
 */
public class ChatMessage {
    private Long id;
    private int memoryId;
    private String role;      // user / assistant
    private String content;
    private int riskLevel;    // 0=正常, 1=注意, 2=警告
    private String scamType;  // 诈骗类型，如为空则表示无
    private LocalDateTime createdAt;

    public ChatMessage() {
    }

    public ChatMessage(int memoryId, String role, String content, int riskLevel, String scamType) {
        this.memoryId = memoryId;
        this.role = role;
        this.content = content;
        this.riskLevel = riskLevel;
        this.scamType = scamType;
        this.createdAt = LocalDateTime.now();
    }

    public Long getId() {
        return id;
    }

    public void setId(Long id) {
        this.id = id;
    }

    public int getMemoryId() {
        return memoryId;
    }

    public void setMemoryId(int memoryId) {
        this.memoryId = memoryId;
    }

    public String getRole() {
        return role;
    }

    public void setRole(String role) {
        this.role = role;
    }

    public String getContent() {
        return content;
    }

    public void setContent(String content) {
        this.content = content;
    }

    public int getRiskLevel() {
        return riskLevel;
    }

    public void setRiskLevel(int riskLevel) {
        this.riskLevel = riskLevel;
    }

    public String getScamType() {
        return scamType;
    }

    public void setScamType(String scamType) {
        this.scamType = scamType;
    }

    public LocalDateTime getCreatedAt() {
        return createdAt;
    }

    public void setCreatedAt(LocalDateTime createdAt) {
        this.createdAt = createdAt;
    }
}