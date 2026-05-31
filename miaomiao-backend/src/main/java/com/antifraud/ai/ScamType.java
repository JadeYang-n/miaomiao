package com.antifraud.ai;

public enum ScamType {
    NONE("无"),
    HEALTH_PRODUCTS("养生保健/神药骗局", new String[]{
        "核心逻辑漏洞",
        "心理操纵手法",
        "典型预警信号",
        "查询关键词"
    }),
    PENSION_INVESTMENT("养老投资骗局", new String[]{
        "核心逻辑漏洞",
        "心理操纵手法",
        "典型预警信号",
        "查询关键词"
    }),
    INVESTMENT_FINANCIAL("投资理财骗局", new String[]{
        "核心逻辑漏洞",
        "心理操纵手法",
        "典型预警信号",
        "查询关键词"
    }),
    SOCIAL_SECURITY("社保医保骗局", new String[]{
        "核心逻辑漏洞",
        "心理操纵手法",
        "典型预警信号",
        "查询关键词"
    }),
    EMOTIONAL("情感类骗局", new String[]{
        "核心逻辑漏洞",
        "心理操纵手法",
        "典型预警信号",
        "查询关键词"
    }),
    FREE_BENEFITS("免费福利骗局", new String[]{
        "核心逻辑漏洞",
        "心理操纵手法",
        "典型预警信号",
        "查询关键词"
    }),
    ART_COLLECTIBLES("收藏艺术品骗局", new String[]{
        "核心逻辑漏洞",
        "心理操纵手法",
        "典型预警信号",
        "查询关键词"
    }),
    NEW_TECH("新型技术骗局", new String[]{
        "核心逻辑漏洞",
        "心理操纵手法",
        "典型预警信号",
        "查询关键词"
    });

    private final String displayName;
    private final String[] knowledgeDims;

    ScamType(String displayName, String[] knowledgeDims) {
        this.displayName = displayName;
        this.knowledgeDims = knowledgeDims;
    }

    ScamType(String displayName) {
        this(displayName, new String[]{});
    }

    public String getDisplayName() {
        return displayName;
    }

    public String[] getKnowledgeDims() {
        return knowledgeDims;
    }
}