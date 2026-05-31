package com.antifraud.repository;

import com.antifraud.entity.ChatMessage;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.jdbc.core.RowMapper;
import org.springframework.stereotype.Repository;

import java.sql.ResultSet;
import java.sql.SQLException;
import java.time.LocalDateTime;
import java.util.List;

/**
 * 聊天消息仓库
 */
@Repository
public class ChatMessageRepository {

    private final JdbcTemplate jdbc;

    public ChatMessageRepository(JdbcTemplate jdbc) {
        this.jdbc = jdbc;
        initTable();
    }

    private void initTable() {
        jdbc.execute("""
            CREATE TABLE IF NOT EXISTS chat_message (
                id BIGINT AUTO_INCREMENT PRIMARY KEY,
                memory_id INT NOT NULL,
                role VARCHAR(20) NOT NULL,
                content TEXT NOT NULL,
                risk_level INT DEFAULT 0,
                scam_type VARCHAR(50),
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """);
    }

    private RowMapper<ChatMessage> rowMapper = new RowMapper<>() {
        public ChatMessage mapRow(ResultSet rs, int rowNum) throws SQLException {
            ChatMessage msg = new ChatMessage();
            msg.setId(rs.getLong("id"));
            msg.setMemoryId(rs.getInt("memory_id"));
            msg.setRole(rs.getString("role"));
            msg.setContent(rs.getString("content"));
            msg.setRiskLevel(rs.getInt("risk_level"));
            msg.setScamType(rs.getString("scam_type"));
            msg.setCreatedAt(rs.getTimestamp("created_at").toLocalDateTime());
            return msg;
        }
    };

    /**
     * 保存消息
     */
    public void save(ChatMessage message) {
        jdbc.update(
            "INSERT INTO chat_message (memory_id, role, content, risk_level, scam_type) VALUES (?, ?, ?, ?, ?)",
            message.getMemoryId(),
            message.getRole(),
            message.getContent(),
            message.getRiskLevel(),
            message.getScamType()
        );
    }

    /**
     * 获取某个 memoryId 的最近 N 条消息
     */
    public List<ChatMessage> findByMemoryId(int memoryId, int limit) {
        return jdbc.query(
            "SELECT * FROM chat_message WHERE memory_id = ? ORDER BY created_at DESC LIMIT ?",
            rowMapper,
            memoryId,
            limit
        );
    }

    /**
     * 获取某个 memoryId 的最近 N 条消息（按时间正序）
     */
    public List<ChatMessage> findRecentByMemoryId(int memoryId, int limit) {
        String sql = """
            SELECT * FROM (
                SELECT * FROM chat_message WHERE memory_id = ? ORDER BY created_at DESC LIMIT ?
            ) t ORDER BY created_at ASC
        """;
        return jdbc.query(sql, rowMapper, memoryId, limit);
    }

    /**
     * 获取某个 memoryId 的所有消息（用于构建 AI 上下文）
     */
    public List<ChatMessage> findAllByMemoryId(int memoryId) {
        return jdbc.query(
            "SELECT * FROM chat_message WHERE memory_id = ? ORDER BY created_at ASC",
            rowMapper,
            memoryId
        );
    }

    /**
     * 统计某个 memoryId 的消息数
     */
    public int countByMemoryId(int memoryId) {
        Integer count = jdbc.queryForObject(
            "SELECT COUNT(*) FROM chat_message WHERE memory_id = ?",
            Integer.class,
            memoryId
        );
        return count != null ? count : 0;
    }

    /**
     * 删除某个 memoryId 的所有消息
     */
    public void deleteByMemoryId(int memoryId) {
        jdbc.update("DELETE FROM chat_message WHERE memory_id = ?", memoryId);
    }

    /**
     * 清理旧的聊天记录，只保留最新的 limit 条消息
     * @param memoryId 要清理的内存 ID
     * @param keepCount 保留的最新消息数量
     */
    public void cleanupOldMessages(int memoryId, int keepCount) {
        String sql = """
            DELETE FROM chat_message
            WHERE memory_id = ?
            AND id NOT IN (
                SELECT id FROM (
                    SELECT id FROM chat_message WHERE memory_id = ?
                    ORDER BY created_at DESC LIMIT ?
                ) t
            )
            """;
        jdbc.update(sql, memoryId, memoryId, keepCount);
    }
}