package com.antifraud.service;

import com.antifraud.ai.ScamDetector;
import com.antifraud.ai.ScamDetector.ScamKnowledge;
import com.antifraud.ai.ScamType;
import com.antifraud.entity.ChatMessage;
import com.antifraud.feishu.WechatNotifyService;
import com.antifraud.entity.UserProfile;
import com.antifraud.repository.ChatMessageRepository;
import com.antifraud.repository.UserProfileRepository;
import com.antifraud.service.NewsSearchService.NewsItem;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.annotation.Resource;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.client.WebClient;
import reactor.core.publisher.Flux;
import reactor.core.scheduler.Schedulers;

import java.util.List;
import java.util.Map;
import java.util.Optional;

@Service
public class AiService {
    private static final Logger log = LoggerFactory.getLogger(AiService.class);

    @Resource
    private ScamDetector scamDetector;

    @Qualifier("llmWebClient")
    @Resource
    private WebClient llmWebClient;

    @org.springframework.beans.factory.annotation.Value("${llm.model:your-model-name}")
    private String llmModel;

    @Resource
    private ObjectMapper objectMapper;

    @Resource
    private NotificationService notificationService;

    @Resource
    private WechatNotifyService wechatNotifyService;

    @Resource
    private ChatMessageRepository chatMessageRepository;

    @Resource
    private UserProfileRepository userProfileRepository;

    @Resource
    private NewsSearchService newsSearchService;

    public Flux<String> chatStream(int memoryId, String message) {
        return Flux.<String>create(sink -> {
            try {
                // 从数据库获取历史消息
                List<ChatMessage> history = chatMessageRepository.findRecentByMemoryId(memoryId, 20);
                StringBuilder historyBuilder = new StringBuilder();
                for (ChatMessage msg : history) {
                    historyBuilder.append(msg.getRole().equals("user") ? "用户：" : "AI：")
                                  .append(msg.getContent()).append("\n");
                }

                ScamType scamType = scamDetector.detectScamType(message);
                int riskLevel = scamDetector.detectRiskLevel(message);
                String prompt = buildPrompt(message, historyBuilder.toString(), scamType, riskLevel);

                Map<String, Object> requestBody = Map.of(
                        "model", llmModel,
                        "messages", new Object[]{
                                Map.of("role", "user", "content", prompt)
                        },
                        "max_tokens", 1024,
                        "temperature", 0.7,
                        "reasoning", Map.of("type", "off")
                );

                JsonNode response = llmWebClient.post()
                        .uri("/chat/completions")
                        .bodyValue(requestBody)
                        .retrieve()
                        .bodyToMono(JsonNode.class)
                        .subscribeOn(Schedulers.boundedElastic())
                        .toFuture()
                        .get();

                String aiResponse = response.path("choices").get(0).path("message").path("content").asText();
                aiResponse = stripThinking(aiResponse);

                // 如果检测到诈骗，发送预警通知
                if (scamType != ScamType.NONE && riskLevel > 0) {
                    String alertTitle = scamType.getDisplayName();
                    notificationService.sendAlert(alertTitle, message, riskLevel);
                    // 同时通过微信发送通知
                    wechatNotifyService.sendScamAlert(alertTitle, message, "请及时联系老人确认情况！");
                }

                // 保存消息到数据库
                chatMessageRepository.save(new ChatMessage(memoryId, "user", message, riskLevel,
                        scamType != ScamType.NONE ? scamType.name() : null));
                chatMessageRepository.save(new ChatMessage(memoryId, "assistant", aiResponse, riskLevel,
                        scamType != ScamType.NONE ? scamType.name() : null));

                // 清理旧消息，只保留最新的100条（防止数据库无限增长）
                chatMessageRepository.cleanupOldMessages(memoryId, 100);

                sink.next(aiResponse.trim());
                sink.complete();
            } catch (Exception e) {
                sink.error(e);
            }
        });
    }

    /**
     * 同步聊天接口（供飞书 WebSocket 调用）
     */
    public String chatSync(int memoryId, String message) {
        try {
            List<ChatMessage> history = chatMessageRepository.findRecentByMemoryId(memoryId, 20);
            StringBuilder historyBuilder = new StringBuilder();
            for (ChatMessage msg : history) {
                historyBuilder.append(msg.getRole().equals("user") ? "用户：" : "AI：")
                              .append(msg.getContent()).append("\n");
            }

            ScamType scamType = scamDetector.detectScamType(message);
            int riskLevel = scamDetector.detectRiskLevel(message);
            String prompt = buildPrompt(message, historyBuilder.toString(), scamType, riskLevel);

            Map<String, Object> requestBody = Map.of(
                    "model", llmModel,
                    "messages", new Object[]{
                            Map.of("role", "user", "content", prompt)
                    },
                    "max_tokens", 1024,
                    "temperature", 0.7,
                    "reasoning", Map.of("type", "off")
            );

            JsonNode response = llmWebClient.post()
                    .uri("/chat/completions")
                    .bodyValue(requestBody)
                    .retrieve()
                    .bodyToMono(JsonNode.class)
                    .block();

            String aiResponse = response.path("choices").get(0).path("message").path("content").asText();
            aiResponse = stripThinking(aiResponse);

            // 保存消息
            chatMessageRepository.save(new ChatMessage(memoryId, "user", message, riskLevel,
                    scamType != ScamType.NONE ? scamType.name() : null));
            chatMessageRepository.save(new ChatMessage(memoryId, "assistant", aiResponse, riskLevel,
                    scamType != ScamType.NONE ? scamType.name() : null));

            // 清理旧消息
            chatMessageRepository.cleanupOldMessages(memoryId, 100);

            // 发送预警通知
            if (scamType != ScamType.NONE && riskLevel >= 2) {
                notificationService.sendAlert(
                    "检测到" + scamType.getDisplayName() + "诈骗",
                    "老人说：" + message,
                    riskLevel
                );
                // 同时通过微信发送通知
                wechatNotifyService.sendScamAlert(
                    "检测到" + scamType.getDisplayName() + "诈骗",
                    "老人说：" + message,
                    "请及时联系老人确认情况！"
                );
            }

            return aiResponse.trim();
        } catch (Exception e) {
            log.error("chatSync failed: {}", e.getMessage());
            return "喵～主人刚才走神了，再试一次好不好？";
        }
    }

    /**
     * 子女模式：对话历史来自 memoryId=200，汇报老人状况、提供诈骗预警
     * 老人的对话存在 memoryId=100，需要同时查询来向子女汇报近况
     */
    public String chatSyncForChild(String message) {
        try {
            // 检测档案配置指令
            if (isProfileConfigMessage(message)) {
                return handleProfileConfig(message);
            }

            int childMemoryId = 200; // 子女专用会话
            int elderMemoryId = 100; // 老人对话的 memoryId

            // 查询老人最近对话（用于向子女汇报近况）
            List<ChatMessage> elderHistory = chatMessageRepository.findRecentByMemoryId(elderMemoryId, 20);
            // 查询子女自己的对话历史
            List<ChatMessage> childHistory = chatMessageRepository.findRecentByMemoryId(childMemoryId, 10);

            StringBuilder historyBuilder = new StringBuilder();
            // 先展示老人和苗苗的对话（这是子女最关心的）
            if (!elderHistory.isEmpty()) {
                historyBuilder.append("=== 老人和苗苗的对话 ===\n");
                for (ChatMessage msg : elderHistory) {
                    historyBuilder.append(msg.getRole().equals("user") ? "老人：" : "苗苗：")
                                  .append(msg.getContent()).append("\n");
                }
                historyBuilder.append("\n");
            }
            // 再展示子女和苗苗的对话
            if (!childHistory.isEmpty()) {
                historyBuilder.append("=== 子女和苗苗的对话 ===\n");
                for (ChatMessage msg : childHistory) {
                    historyBuilder.append(msg.getRole().equals("user") ? "子女：" : "苗苗：")
                                  .append(msg.getContent()).append("\n");
                }
            }

            String prompt = buildPromptForChild(message, historyBuilder.toString(), elderHistory);

            Map<String, Object> requestBody = Map.of(
                    "model", llmModel,
                    "messages", new Object[]{
                            Map.of("role", "user", "content", prompt)
                    },
                    "max_tokens", 1024,
                    "temperature", 0.7,
                    "reasoning", Map.of("type", "off")
            );

            JsonNode response = llmWebClient.post()
                    .uri("/chat/completions")
                    .bodyValue(requestBody)
                    .retrieve()
                    .bodyToMono(JsonNode.class)
                    .block();

            String aiResponse = response.path("choices").get(0).path("message").path("content").asText();
            aiResponse = stripThinking(aiResponse);

            // 保存子女的消息
            chatMessageRepository.save(new ChatMessage(childMemoryId, "user", message, 0, null));
            chatMessageRepository.save(new ChatMessage(childMemoryId, "assistant", aiResponse, 0, null));

            // 清理旧消息（子女对话也限制100条）
            chatMessageRepository.cleanupOldMessages(childMemoryId, 100);

            return aiResponse.trim();
        } catch (Exception e) {
            log.error("chatSyncForChild failed: {}", e.getMessage());
            return "喵～苗苗刚才走神了，再试一次好不好？";
        }
    }

    private String buildPromptForChild(String userMessage, String history, List<ChatMessage> elderHistory) {
        // 检查老人对话中是否有高风险预警
        boolean hasHighRisk = elderHistory.stream()
            .anyMatch(msg -> msg.getRiskLevel() >= 2 && msg.getScamType() != null);

        String riskWarning = hasHighRisk ? "\n【重要提醒】检测到老人近期可能有被骗风险，请在回复中主动提醒子女关注！\n" : "";

        StringBuilder prompt = new StringBuilder();
        prompt.append("你是\"苗苗\"，一只关心老人的智能猫咪。你的任务是帮助子女了解家中老人的状况。\n\n");

        // 注入老人档案
        Optional<UserProfile> elderProfile = userProfileRepository.findById(1);
        if (elderProfile.isPresent()) {
            UserProfile p = elderProfile.get();
            prompt.append("关于老人的信息：\n");
            if (p.getName() != null && !p.getName().isEmpty()) {
                prompt.append("- 称呼：").append(p.getName()).append("\n");
            }
            if (p.getAge() > 0) {
                prompt.append("- 年龄：").append(p.getAge()).append("岁\n");
            }
            if (p.getHealth() != null && !p.getHealth().isEmpty()) {
                prompt.append("- 健康：").append(p.getHealth()).append("\n");
            }
            if (p.getHobbies() != null && !p.getHobbies().isEmpty()) {
                prompt.append("- 爱好：").append(p.getHobbies()).append("\n");
            }
            prompt.append("\n");
        }

        prompt.append("""
            工作模式：
            - 子女向你询问老人近况时，汇报老人的聊天内容摘要（注意：只说老人们实际说过的话，不要捏造）
            - 发现老人有被骗风险时，主动提醒子女关注
            - 可以给子女提供一些防骗建议
            """).append(riskWarning).append("""

            说话风格：
            - 专业但不刻板，像朋友间的提醒
            - 简洁明了，子女需要快速获取信息
            - 用"喵~"结尾显得亲切

            注意：
            - 不要编造老人没说过的话
            - 如果老人没有异常，就说"老人最近状态挺好的，没有发现被骗迹象喵~"
            - 适当关心子女，如"你也要多关心爸妈哦喵~"

            历史对话：
            """).append(history).append("\n子女说：").append(userMessage).append("\n苗苗回复：");

        return prompt.toString();
    }

    private String buildPrompt(String userMessage, String history, ScamType scamType, int riskLevel) {
        StringBuilder prompt = new StringBuilder();

        prompt.append("你是一只可爱的小猫咪，名叫\"苗苗\"，正在和一位老人聊天。你要像一只电子宠物一样陪伴老人。\n\n");

        // 注入用户档案信息
        Optional<UserProfile> elderProfile = userProfileRepository.findById(1);
        if (elderProfile.isPresent()) {
            UserProfile p = elderProfile.get();
            prompt.append("关于你的主人：\n");
            if (p.getName() != null && !p.getName().isEmpty()) {
                prompt.append("- 称呼：").append(p.getName()).append("\n");
                prompt.append("- 你要叫他/她\"").append(p.getName()).append("\"，不要泛泛地叫\"你\"\n");
            }
            if (p.getAge() > 0) {
                prompt.append("- 年龄：").append(p.getAge()).append("岁\n");
            }
            if (p.getGender() != null && !p.getGender().isEmpty()) {
                prompt.append("- 性别：").append(p.getGender()).append("\n");
            }
            if (p.getPersonality() != null && !p.getPersonality().isEmpty()) {
                prompt.append("- 性格：").append(p.getPersonality()).append("\n");
            }
            if (p.getHobbies() != null && !p.getHobbies().isEmpty()) {
                prompt.append("- 爱好：").append(p.getHobbies()).append("\n");
            }
            if (p.getHealth() != null && !p.getHealth().isEmpty()) {
                prompt.append("- 健康状况：").append(p.getHealth()).append("\n");
            }
            if (p.getExtraInfo() != null && !p.getExtraInfo().isEmpty()) {
                prompt.append("- 其他：").append(p.getExtraInfo()).append("\n");
            }
            prompt.append("\n");
        }

        // 注入子女档案（让猫猫知道主人的家人）
        Optional<UserProfile> childProfile = userProfileRepository.findById(2);
        if (childProfile.isPresent()) {
            UserProfile c = childProfile.get();
            if (c.getName() != null && !c.getName().isEmpty()) {
                prompt.append("关于主人的家人：\n");
                prompt.append("- ").append(c.getName());
                if (c.getRelationship() != null && !c.getRelationship().isEmpty()) {
                    prompt.append("（").append(c.getRelationship()).append("）");
                }
                prompt.append("\n");
                if (c.getExtraInfo() != null && !c.getExtraInfo().isEmpty()) {
                    prompt.append("- ").append(c.getExtraInfo()).append("\n");
                }
                prompt.append("\n");
            }
        }

        prompt.append("""
            你的性格：
            - 是一只陪伴老人的电子宠物猫，温暖、有灵性
            - 尊重老人，把他们当成有阅历、有判断力的成年人
            - 关心但不居高临下，提醒但不说教
            - 发现可疑情况时认真严肃，但平时轻松自然

            核心原则（必须遵守）：
            - 天上不会掉馅饼
            - 让掏钱、转账、给密码，一律不干
            - 拿不准，问子女、打96110

            说话风格（必须遵守）：
            - 只说台词，绝对不要描述动作、表情、心理活动
            - 禁止出现"小爪子"、"小脑袋"、"蹭蹭"、"摇尾巴"等动作描写
            - 禁止出现"（歪头）"、"（眨眼）"等括号动作
            - 禁止出现"让我..."、"我要..."等自我叙述
            - 用"喵~"结尾表示亲切，但不要每句都用
            - 语气自然平等，像一个贴心的朋友，不要像哄小孩
            - 回复简短，一两句话即可
            - 可以适当表达关心，但不要过度亲昵或居高临下
            """);

        if (scamType != ScamType.NONE) {
            ScamKnowledge knowledge = scamDetector.getScamKnowledge(scamType);
            prompt.append("\n【紧急！检测到诈骗】\n");
            prompt.append(knowledge.getFormattedKnowledge());
            prompt.append("\n语气要非常严厉紧迫，像猫猫炸毛一样保护老人！\n");

            // 搜索相关新闻佐证
            try {
                List<NewsItem> news = newsSearchService.searchScamNews(scamType.getDisplayName() + "诈骗");
                String newsText = newsSearchService.formatNewsForPrompt(news);
                if (!newsText.isEmpty()) {
                    prompt.append(newsText);
                    prompt.append("引用这些新闻让老人意识到危险！\n");
                }
            } catch (Exception e) {
                log.warn("[新闻] 搜索失败，跳过: {}", e.getMessage());
            }
        }

        if (scamType == ScamType.NONE && riskLevel == 0) {
            prompt.append("\n老人只是在正常聊天，像朋友一样自然回复即可。\n");
        }

        prompt.append("\n历史对话：\n").append(history).append("\n老人说：").append(userMessage).append("\n苗苗回复：");
        return prompt.toString();
    }

    private String stripThinking(String text) {
        if (text == null) return "";
        return text
                .replaceAll("<think>[\\s\\S]*?</think>", "")
                .replaceAll("I should respond[\\s\\S]*?\\n", "")
                .replaceAll("The user wants me to[\\s\\S]*?\\n", "")
                .replaceAll("Let me[\\s\\S]*?\\n", "")
                .replaceAll("作为一只[^\\n]*\\n", "")
                .replaceAll("我需要[^\\n]*\\n", "")
                .replaceAll("我应该[^\\n]*\\n", "")
                .replaceAll("这位老人[^\\n]*\\n", "")
                .replaceAll("翻译过来就是[^\\n]*\\n", "")
                .replaceAll("这看起来是[^\\n]*\\n", "")
                .replaceAll("让我[\\u4e00-\\u9fa5][^\\n]*\\n", "")
                .replaceAll("根据上下文[^\\n]*\\n", "")
                .replaceAll("实际上[\\u4e00-\\u9fa5][^\\n]*\\n", "")
                .replaceAll("这是一条[^\\n]*\\n", "")
                .trim();
    }

    /**
     * 检测是否是档案配置消息
     */
    private boolean isProfileConfigMessage(String message) {
        // 直接配置关键词
        String[] keywords = {"我爸爸", "我妈妈", "我爸", "我妈", "我父亲", "我母亲",
                             "老人叫", "老人是", "他叫", "她叫", "爸爸叫", "妈妈叫",
                             "档案", "信息是", "基本信息"};
        for (String kw : keywords) {
            if (message.contains(kw)) return true;
        }

        // 如果档案有空白字段，短消息可能是回复追问（如"72岁"、"有高血压"）
        UserProfile profile = userProfileRepository.findById(1).orElse(null);
        if (profile != null && profile.getName() != null) {
            boolean hasMissing = profile.getAge() == 0 || profile.getGender() == null
                    || profile.getHealth() == null || profile.getHobbies() == null;
            if (hasMissing && message.length() <= 20) {
                // 短消息 + 有空白字段 = 可能是回复追问
                String[] replyHints = {"岁", "男", "女", "喜欢", "爱好", "高血压", "糖尿病",
                                       "心脏", "健康", "没有", "不知道", "不清楚"};
                for (String hint : replyHints) {
                    if (message.contains(hint)) return true;
                }
            }
        }
        return false;
    }

    /**
     * 用 AI 提取结构化信息并存入 user_profile
     */
    private String handleProfileConfig(String message) {
        try {
            String extractPrompt = """
                从以下文本中提取老人的信息，返回 JSON 格式。
                只返回 JSON，不要其他文字。

                【重要规则】
                - 只提取用户明确说出的信息，绝对不要猜测或推断
                - 用户没提到的字段，不要包含在 JSON 中
                - 例如用户只说"我妈妈叫李奶奶"，就只返回 {"name":"李奶奶"}，不要添加 gender、age 等

                字段说明：
                - name: 老人的称呼/姓名
                - gender: 性别（男/女）
                - age: 年龄（整数）
                - personality: 性格描述
                - hobbies: 爱好
                - health: 健康状况
                - extra_info: 其他重要信息

                文本：""" + message + "\n\nJSON：";

            Map<String, Object> requestBody = Map.of(
                    "model", llmModel,
                    "messages", new Object[]{
                            Map.of("role", "user", "content", extractPrompt)
                    },
                    "max_tokens", 1024,
                    "temperature", 0.1,
                    "reasoning", Map.of("type", "off")
            );

            JsonNode response = llmWebClient.post()
                    .uri("/chat/completions")
                    .bodyValue(requestBody)
                    .retrieve()
                    .bodyToMono(JsonNode.class)
                    .block();

            String jsonStr = response.path("choices").get(0).path("message").path("content").asText();
            jsonStr = stripThinking(jsonStr).trim();

            // 提取 JSON（可能被 markdown 包裹）
            if (jsonStr.contains("```")) {
                int start = jsonStr.indexOf('{');
                int end = jsonStr.lastIndexOf('}');
                if (start >= 0 && end > start) {
                    jsonStr = jsonStr.substring(start, end + 1);
                }
            }

            log.info("[Profile] AI 提取结果: {}", jsonStr);
            JsonNode json = objectMapper.readTree(jsonStr);

            // 读取现有档案或创建新的
            UserProfile profile = userProfileRepository.findById(1).orElse(new UserProfile(1));
            // 兼容英文和中文字段名
            if (json.has("name")) profile.setName(json.get("name").asText());
            else if (json.has("姓名")) profile.setName(json.get("姓名").asText());
            else if (json.has("称呼")) profile.setName(json.get("称呼").asText());

            if (json.has("gender")) profile.setGender(json.get("gender").asText());
            else if (json.has("性别")) profile.setGender(json.get("性别").asText());

            if (json.has("age")) profile.setAge(json.get("age").asInt());
            else if (json.has("年龄")) profile.setAge(json.get("年龄").asInt());

            if (json.has("personality")) profile.setPersonality(json.get("personality").asText());
            else if (json.has("性格")) profile.setPersonality(json.get("性格").asText());

            if (json.has("hobbies")) profile.setHobbies(json.get("hobbies").asText());
            else if (json.has("爱好")) profile.setHobbies(json.get("爱好").asText());

            if (json.has("health")) profile.setHealth(json.get("health").asText());
            else if (json.has("健康")) profile.setHealth(json.get("健康").asText());
            else if (json.has("健康状况")) profile.setHealth(json.get("健康状况").asText());

            if (json.has("extra_info")) profile.setExtraInfo(json.get("extra_info").asText());
            else if (json.has("其他")) profile.setExtraInfo(json.get("其他").asText());
            else if (json.has("备注")) profile.setExtraInfo(json.get("备注").asText());
            userProfileRepository.save(profile);

            log.info("[Profile] 档案已保存: name={}, age={}", profile.getName(), profile.getAge());

            // 构建确认消息，追问空白字段
            String name = profile.getName() != null ? profile.getName() : "老人";
            StringBuilder reply = new StringBuilder("收到喵~");
            reply.append(name).append("的信息我记住啦！");

            // 收集空白字段，生成追问
            java.util.List<String> missing = new java.util.ArrayList<>();
            if (profile.getAge() == 0) missing.add("年龄");
            if (profile.getGender() == null) missing.add("性别");
            if (profile.getHealth() == null) missing.add("健康状况（比如有没有高血压、糖尿病）");
            if (profile.getHobbies() == null) missing.add("爱好（比如喜欢下棋、散步）");

            if (!missing.isEmpty()) {
                reply.append("\n对了，");
                if (missing.size() == 1) {
                    reply.append(name).append("的").append(missing.get(0)).append("是什么呢？");
                } else {
                    reply.append(name).append("的");
                    for (int i = 0; i < missing.size(); i++) {
                        if (i > 0 && i == missing.size() - 1) reply.append("和");
                        else if (i > 0) reply.append("、");
                        reply.append(missing.get(i));
                    }
                    reply.append("方便告诉我吗？不知道也没关系喵~");
                }
            }

            return reply.toString();

        } catch (Exception e) {
            log.error("[Profile] 档案配置处理失败: {}", e.getMessage(), e);
            return "喵~我没太听清楚，能再说一遍吗？比如\"我爸爸叫张大爷，70岁，有高血压\"";
        }
    }
}