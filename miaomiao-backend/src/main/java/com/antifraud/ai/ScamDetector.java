package com.antifraud.ai;

import org.springframework.stereotype.Component;

import java.util.*;

@Component
public class ScamDetector {

    private final Map<Integer, Set<String>> riskKeywords = new HashMap<>();
    private final Map<ScamType, ScamKnowledge> scamKnowledge = new HashMap<>();

    public ScamDetector() {
        // 高风险关键词（风险等级2）
        riskKeywords.put(2, Set.of(
                "转账", "汇款", "验证码", "银行卡", "密码", "账号", "账户",
                "投资", "理财", "高收益", "快速致富", "稳赚不赔", "内幕消息",
                "中奖", "领奖", "免费", "限时", "紧急", "马上", "现在"
        ));

        // 中等风险关键词（风险等级1）
        riskKeywords.put(1, Set.of(
                "保健品", "养生", "延年益寿", "增强免疫力", "抗衰老",
                "养老", "养老服务", "养老公寓", "养老金", "养老保险",
                "股票", "基金", "期货", "外汇", "数字货币",
                "兼职", "刷单", "返利", "返现", "优惠券"
        ));

        // 初始化各类型知识库
        initHealthProductsScam();
        initPensionInvestmentScam();
        initInvestmentFinancialScam();
        initSocialSecurityScam();
        initEmotionalScam();
        initFreeBenefitsScam();
        initArtCollectiblesScam();
        initNewTechScam();
    }

    private void initHealthProductsScam() {
        scamKnowledge.put(ScamType.HEALTH_PRODUCTS, new ScamKnowledge(
                ScamType.HEALTH_PRODUCTS,
                new String[]{
                        "核心逻辑漏洞：",
                        "1. 如果产品真能包治百病或延年益寿，早就通过正规医院和药店销售了，不需要上门推销。",
                        "2. 药品上市需要国家药监局审批，而保健品不是药品，不具备治疗功能。",
                        "3. '祖传秘方'、'诺贝尔奖技术'、'航天员专用'等虚假宣传，利用信息不对称欺骗老人。",
                        "4. 卖家敢在小区门口卖，却不敢让你孩子查，说明心里有鬼。",
                        "",
                        "心理操纵手法：",
                        "1. 利用老人对疾病的恐惧和对健康的渴望（怕死、怕给子女添麻烦）。",
                        "2. 打'亲情牌'：干儿子干女儿称呼、端茶倒水、认老人做干爹干妈。",
                        "3. '限时优惠'、'名额有限'制造紧迫感，让人来不及和家人商量。",
                        "4. 利用从众心理：找托儿排队、说'隔壁王奶奶都买了'。",
                        "",
                        "典型预警信号：",
                        "1. 上门推销或电话推销，不给你考虑时间。",
                        "2. 宣称'食品/保健品能治病'、'能停药'。",
                        "3. 退钱容易（因为根本没打算让你退，主要靠'会员费'或'升级费'赚钱）。",
                        "4. 现场让你刷卡或付现金，不敢走电商平台。",
                        "",
                        "查询关键词：保健品诈骗 老人 案例、上门推销保健品骗局、保健品洗脑话术"
                }
        ));
    }

    private void initPensionInvestmentScam() {
        scamKnowledge.put(ScamType.PENSION_INVESTMENT, new ScamKnowledge(
                ScamType.PENSION_INVESTMENT,
                new String[]{
                        "核心逻辑漏洞：",
                        "1. 银行一年期定存利率约1.5%-2%，他说年化10%，他做啥生意能赚这么多？肯定是骗本金。",
                        "2. 正规养老机构床位费是月付或季付，不需要一次性投入几十万。",
                        "3. '售后返租'、'高息返利'本质是借新还旧，资金链必断。",
                        "4. 房产是老人最后的保障，以房养老正规产品是政府试点的，不是民间高息。",
                        "",
                        "心理操纵手法：",
                        "1. 利用老人'攒钱养老'的心理，编造'专门为老人设计的理财'。",
                        "2. 组织免费旅游、参观养老公寓，让老人沉浸式感受'美好未来'。",
                        "3. 第一二期准时返利（老人尝到甜头后加大投入）。",
                        "4. 装正经公司，请领导合影、租高大上办公室。",
                        "",
                        "典型预警信号：",
                        "1. 承诺保本+高息，比银行理财高5倍以上。",
                        "2. 投资款打入个人账户或非养老机构对公账户。",
                        "3. 合同条款模糊、规避责任，说'这是行业惯例'。",
                        "4. 让你瞒着子女，说'孩子知道了会反对，错过好机会'。",
                        "",
                        "查询关键词：养老投资诈骗 非法集资 案例、以房养老骗局、养老公寓非法吸收公众存款"
                }
        ));
    }

    private void initInvestmentFinancialScam() {
        scamKnowledge.put(ScamType.INVESTMENT_FINANCIAL, new ScamKnowledge(
                ScamType.INVESTMENT_FINANCIAL,
                new String[]{
                        "核心逻辑漏洞：",
                        "1. '原始股'：能上市的公司股权不会流向散户，能流向散户的'原始股'基本是虚假融资。",
                        "2. '内部消息'：如果真有内幕消息，人家自己偷偷买就行了，为什么要告诉陌生人？",
                        "3. '稳赚不赔'：投资有风险，正规金融机构不准承诺保本，这是刑法明确规定的。",
                        "4. 虚拟币本身就是高风险投机品，国内禁止虚拟币交易，所谓的'交易所'不受法律保护。",
                        "",
                        "心理操纵手法：",
                        "1. '老师带单'：先让你小赚，建立信任后再重仓亏损。",
                        "2. 晒盈利截图、群里的'赚钱截图'都是托儿。",
                        "3. 入金容易出金难：赚的钱永远显示在账户里，提现时发现根本提不出来。",
                        "4. 正规基金有监管，骗子平台随时可关闭。",
                        "",
                        "典型预警信号：",
                        "1. 被拉入炒股群、炒外汇群、炒币群，群里天天晒盈利。",
                        "2. 所谓的'老师'让你加仓、止损，从来不让你止损。",
                        "3. 平台要求你把资金打入个人账户或'指定账户'。",
                        "4. 收益高得不正常，且宣传'本金安全'。",
                        "",
                        "查询关键词：荐股诈骗 杀猪盘 案例、虚假投资平台 诈骗套路、原始股骗局 非法集资"
                }
        ));
    }

    private void initSocialSecurityScam() {
        scamKnowledge.put(ScamType.SOCIAL_SECURITY, new ScamKnowledge(
                ScamType.SOCIAL_SECURITY,
                new String[]{
                        "核心逻辑漏洞：",
                        "1. 全国几亿人交社保，能'走后门'的国家早挤爆了，轮不到找你'帮忙'。",
                        "2. 社保政策由国家制定，地方人社局无权私自'放宽条件'或'提前退休'。",
                        "3. 社保补缴有严格政策，必须符合特定条件，且通过官方窗口办理。",
                        "4. 真正的社保业务不会电话通知、更不会让你转账。",
                        "",
                        "心理操纵手法：",
                        "1. 冒充社保局工作人员，用真实的政策条文让你信以为真。",
                        "2. '最后三天'、'名额有限'制造紧迫感。",
                        "3. 先收'代办费'，再收'加急费'，层层加码。",
                        "4. 让你觉得'这是正规渠道，只是找关系而已'。",
                        "",
                        "典型预警信号：",
                        "1. 来电自称社保局、人社局，说你社保卡异常或可以补缴。",
                        "2. 要求你转账到'安全账户'或'指定账户'。",
                        "3. 索要你的身份证、银行卡、验证码。",
                        "4. 强调'不能告诉其他人'、'这是保密的'。",
                        "",
                        "查询关键词：冒充社保局诈骗 案例、社保卡异常诈骗套路、办理退休诈骗"
                }
        ));
    }

    private void initEmotionalScam() {
        scamKnowledge.put(ScamType.EMOTIONAL, new ScamKnowledge(
                ScamType.EMOTIONAL,
                new String[]{
                        "核心逻辑漏洞：",
                        "1. 你的亲生子女都不一定天天陪你，一个'干儿子'比亲儿子还亲，图什么？",
                        "2. 警察办案有严格程序，不会电话通知转钱，也没有'安全账户'。",
                        "3. '被绑架'、'出车祸'等紧急情况，亲人会直接报警，不会让你转账。",
                        "4. 真正遇到事的家人，会哭着说、会让你核实，不会让你保密、不让你挂电话。",
                        "",
                        "心理操纵手法：",
                        "1. 黄昏恋骗局：网上认识，迅速确立关系，以各种理由要钱。",
                        "2. 冒充亲友（孩子/孙辈）：声音像、知道名字，利用老年人心疼晚辈的心理。",
                        "3. 制造紧急情况让你来不及思考：'妈，我出事了，快打钱'。",
                        "4. 让你独处，切断你和家人的联系通道。",
                        "",
                        "典型预警信号：",
                        "1. 对方拒绝视频通话，或通话时声音模糊、场景可疑。",
                        "2. 以各种理由要钱：生意周转、出事需要保释、手术费。",
                        "3. 强调'不要告诉其他人'、'说了会挨骂'。",
                        "4. 让你独处、或让你去酒店、网吧等避开家人。",
                        "",
                        "查询关键词：黄昏恋诈骗 杀猪盘 案例、冒充亲友诈骗 老人、电话诈骗 孙子出事 案例"
                }
        ));
    }

    private void initFreeBenefitsScam() {
        scamKnowledge.put(ScamType.FREE_BENEFITS, new ScamKnowledge(
                ScamType.FREE_BENEFITS,
                new String[]{
                        "核心逻辑漏洞：",
                        "1. 天下没有白吃的午餐，免费送鸡蛋/米面的成本，最终要从你身上成倍赚回来。",
                        "2. 几百块的'低价旅游'连路费都不够，旅行社怎么赚钱？靠强制购物。",
                        "3. 免费体检、免费讲座的目的，是筛选易骗对象，后续定向推销。",
                        "4. 送东西的人不是慈善机构，是销售员，工资靠你买单。",
                        "",
                        "心理操纵手法：",
                        "1. 用小恩小惠把老人吸引到会场，再进行洗脑式推销。",
                        "2. 现场营造'大家都在买'的氛围，托儿带头。",
                        "3. '专家'站台、'领导'合影增加可信度。",
                        "4. 连续几天送东西，建立信任后再推出'大单'。",
                        "",
                        "典型预警信号：",
                        "1. 接到电话/短信说'恭喜你中奖'、'免费领礼品'。",
                        "2. 被拉到保健品公司、旅游公司参加'活动'。",
                        "3. 对方回避价格，一直强调'今天特价'、'名额有限'。",
                        "4. 让你瞒着子女，说'别让孩子知道，他们会多想的'。",
                        "",
                        "查询关键词：保健品会销 老人被骗、免费旅游 强制购物 骗局、免费体检 诈骗套路"
                }
        ));
    }

    private void initArtCollectiblesScam() {
        scamKnowledge.put(ScamType.ART_COLLECTIBLES, new ScamKnowledge(
                ScamType.ART_COLLECTIBLES,
                new String[]{
                        "核心逻辑漏洞：",
                        "1. 真能升值的东西（古董、字画、限量邮票），要么在拍卖行，要么在专业藏家手里，不会让你在小摊上买到。",
                        "2. 如果真能高价回收，卖家自己留着明年卖就行了，为什么要便宜卖给你？",
                        "3. '帮你拍卖'、'帮你增值'的公司，收完服务费后人去楼空。",
                        "4. 所谓'限量发行'、'收藏价值'的纪念币、邮票，实际价值远低于售价。",
                        "",
                        "心理操纵手法：",
                        "1. 打'文化'、'传承'牌，让老人觉得买了是有品位、有文化。",
                        "2. 谎称'这套是给领导/收藏家预留的'，制造稀缺感。",
                        "3. 提供'鉴定证书'，但鉴定机构本身就是骗子设的。",
                        "4. 承诺几年后高价回收，但合同里写满了免责条款。",
                        "",
                        "典型预警信号：",
                        "1. 在街头、讲座、旅游景点被推销'收藏品'。",
                        "2. 对方声称'这套以后肯定大涨'、'国家会回收'。",
                        "3. 要求现金支付，不给发票或合同条款模糊。",
                        "4. 强调'现在不买就没有了'、'以后买不到了'。",
                        "",
                        "查询关键词：收藏品诈骗 老人、古董鉴定 骗局、纪念币投资 诈骗"
                }
        ));
    }

    private void initNewTechScam() {
        scamKnowledge.put(ScamType.NEW_TECH, new ScamKnowledge(
                ScamType.NEW_TECH,
                new String[]{
                        "核心逻辑漏洞：",
                        "1. 正规APP不会在短信/链接里让你下载，都是从应用商店下载。",
                        "2. 运营商、公检法不会电话要你的验证码。验证码就是密码，给了等于给钱。",
                        "3. AI变声模仿再像，声音不等于本人，必须有其他核实手段。",
                        "4. '中奖'是最低级的骗局，没参加抽奖不会中奖，参加了也是假的。",
                        "",
                        "心理操纵手法：",
                        "1. 冒充公检法：说你'涉嫌洗钱'、'账户被冻结'，用恐惧让人失去判断力。",
                        "2. 冒充客服：说你'订单异常'、'可以退款'，引导你转账。",
                        "3. 短信钓鱼：点击链接后窃取账号密码。",
                        "4. 屏幕共享：让你安装APP远程控制，窃取银行信息。",
                        "",
                        "典型预警信号：",
                        "1. 收到陌生短信/电话，说你'中奖'、'账户异常'、'快递有问题'。",
                        "2. 对方能准确报出你的个人信息（号码、地址、订单信息）。",
                        "3. 让你转账到'安全账户'、'指定账户'，或提供验证码/密码。",
                        "4. 让你独处接电话，或去酒店/网吧操作，避开家人。",
                        "",
                        "查询关键词：电信诈骗 验证码、AI变声诈骗 案例、冒充公检法诈骗 套路、短信钓鱼 诈骗"
                }
        ));
    }

    public int detectRiskLevel(String message) {
        if (message == null || message.isEmpty()) {
            return 0;
        }

        if (containsAnyKeyword(message, riskKeywords.get(2))) {
            return 2;
        }

        if (containsAnyKeyword(message, riskKeywords.get(1))) {
            return 1;
        }

        return 0;
    }

    /**
     * 检测诈骗类型
     */
    public ScamType detectScamType(String message) {
        if (message == null || message.isEmpty()) {
            return ScamType.NONE;
        }

        // 按优先级检测：情感类(最高危) > 投资理财 > 养老 > 社保 > 神药 > 免费 > 收藏 > 新型
        ScamType[] priority = {
                ScamType.EMOTIONAL,
                ScamType.INVESTMENT_FINANCIAL,
                ScamType.PENSION_INVESTMENT,
                ScamType.SOCIAL_SECURITY,
                ScamType.HEALTH_PRODUCTS,
                ScamType.FREE_BENEFITS,
                ScamType.ART_COLLECTIBLES,
                ScamType.NEW_TECH
        };

        for (ScamType type : priority) {
            if (containsAnyKeyword(message, getKeywordsForType(type))) {
                return type;
            }
        }

        return ScamType.NONE;
    }

    /**
     * 获取指定诈骗类型的关键词集合
     */
    private Set<String> getKeywordsForType(ScamType type) {
        // 各类型基础关键词，用于触发检测
        return switch (type) {
            case HEALTH_PRODUCTS -> Set.of(
                    "保健品", "养生", "神药", "包治百病", "延年益寿", "增强免疫力",
                    "理疗仪", "健康讲座", "免费体检", "三高", "心脑血管", "糖尿病",
                    "营养品", "燕窝", "灵芝", "玛卡", "氨糖", "钙片", "蜂胶"
            );
            case PENSION_INVESTMENT -> Set.of(
                    "养老公寓", "养老院", "床位", "以房养老", "高息", "返利",
                    "保本", "售后返租", "养老投资", "养老服务", "养老社区",
                    "养生基地", "候鸟式养老", "会员卡", "床位费"
            );
            case INVESTMENT_FINANCIAL -> Set.of(
                    "股票", "原始股", "基金", "期货", "外汇", "虚拟币", "数字货币",
                    "国债", "内部理财", "高回报", "稳赚", "投资", "炒股", "配资",
                    "杠杆", "合约", "K线", "涨停", "牛股", "庄家"
            );
            case SOCIAL_SECURITY -> Set.of(
                    "社保", "医保", "补缴", "代办", "提前退休", "养老金",
                    "提高养老金", "医保报销", "社保局", "失业金", "生育险"
            );
            case EMOTIONAL -> Set.of(
                    "干儿子", "干女儿", "黄昏恋", "谈恋爱", "结婚", "借钱",
                    "出事", "被绑架", "车祸", "生病", "转账", "保密",
                    "女朋友", "男朋友", "网友", "主播", "恋爱"
            );
            case FREE_BENEFITS -> Set.of(
                    "鸡蛋", "米面", "旅游", "低价游", "旅行团",
                    "领礼品", "讲座", "推销", "赠送", "免费领", "领鸡蛋"
            );
            case ART_COLLECTIBLES -> Set.of(
                    "古董", "字画", "纪念币", "收藏", "增值", "拍卖", "回收",
                    "鉴定", "保值", "限量", "绝版", "收藏品", "钱币", "邮票"
            );
            case NEW_TECH -> Set.of(
                    "中奖", "积分兑换", "APP", "下载", "AI变声", "视频",
                    "验证码", "安全账户", "资金冻结", "客服", "订单异常",
                    "快递", "包裹", "理赔", "退款", "刮刮卡"
            );
            default -> Collections.emptySet();
        };
    }

    /**
     * 获取诈骗知识库
     */
    public ScamKnowledge getScamKnowledge(ScamType type) {
        return scamKnowledge.get(type);
    }

    private boolean containsAnyKeyword(String message, Set<String> keywords) {
        for (String keyword : keywords) {
            if (message.contains(keyword)) {
                return true;
            }
        }
        return false;
    }

    /**
     * 诈骗知识结构
     */
    public static class ScamKnowledge {
        private final ScamType type;
        private final String[] knowledgePoints;

        public ScamKnowledge(ScamType type, String[] knowledgePoints) {
            this.type = type;
            this.knowledgePoints = knowledgePoints;
        }

        public ScamType getType() {
            return type;
        }

        public String[] getKnowledgePoints() {
            return knowledgePoints;
        }

        public String getFormattedKnowledge() {
            return String.join("\n", knowledgePoints);
        }
    }
}