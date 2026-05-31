package com.antifraud.repository;

import com.antifraud.entity.UserProfile;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.jdbc.core.RowMapper;
import org.springframework.stereotype.Repository;

import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.Optional;

/**
 * 用户档案仓库
 */
@Repository
public class UserProfileRepository {

    private final JdbcTemplate jdbc;

    public UserProfileRepository(JdbcTemplate jdbc) {
        this.jdbc = jdbc;
        initTable();
    }

    private void initTable() {
        jdbc.execute("""
            CREATE TABLE IF NOT EXISTS user_profile (
                id INT PRIMARY KEY,
                name VARCHAR(50),
                gender VARCHAR(10),
                age INT DEFAULT 0,
                personality TEXT,
                hobbies TEXT,
                health TEXT,
                relationship VARCHAR(50),
                contact_method VARCHAR(100),
                extra_info TEXT,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """);
    }

    private RowMapper<UserProfile> rowMapper = new RowMapper<>() {
        public UserProfile mapRow(ResultSet rs, int rowNum) throws SQLException {
            UserProfile p = new UserProfile();
            p.setId(rs.getInt("id"));
            p.setName(rs.getString("name"));
            p.setGender(rs.getString("gender"));
            p.setAge(rs.getInt("age"));
            p.setPersonality(rs.getString("personality"));
            p.setHobbies(rs.getString("hobbies"));
            p.setHealth(rs.getString("health"));
            p.setRelationship(rs.getString("relationship"));
            p.setContactMethod(rs.getString("contact_method"));
            p.setExtraInfo(rs.getString("extra_info"));
            java.sql.Timestamp ts = rs.getTimestamp("updated_at");
            if (ts != null) p.setUpdatedAt(ts.toLocalDateTime());
            return p;
        }
    };

    /**
     * 查询档案
     */
    public Optional<UserProfile> findById(int id) {
        var list = jdbc.query("SELECT * FROM user_profile WHERE id = ?", rowMapper, id);
        return list.isEmpty() ? Optional.empty() : Optional.of(list.get(0));
    }

    /**
     * 插入或更新档案
     */
    public void save(UserProfile profile) {
        profile.setUpdatedAt(java.time.LocalDateTime.now());
        jdbc.update("""
            MERGE INTO user_profile (id, name, gender, age, personality, hobbies, health, relationship, contact_method, extra_info, updated_at)
            KEY(id) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
            profile.getId(),
            profile.getName(),
            profile.getGender(),
            profile.getAge(),
            profile.getPersonality(),
            profile.getHobbies(),
            profile.getHealth(),
            profile.getRelationship(),
            profile.getContactMethod(),
            profile.getExtraInfo(),
            java.sql.Timestamp.valueOf(profile.getUpdatedAt())
        );
    }

    /**
     * 删除档案
     */
    public void deleteById(int id) {
        jdbc.update("DELETE FROM user_profile WHERE id = ?", id);
    }
}
