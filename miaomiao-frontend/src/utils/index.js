// 生成唯一的内存ID
// 老人端固定使用 memoryId = 100，方便子女通过飞书查询老人近况
export function generateMemoryId() {
  return 100  // 固定为100，便于飞书子女模式读取老人对话历史
}

// 格式化时间
export function formatTime(date) {
  const now = new Date(date)
  const hours = now.getHours().toString().padStart(2, '0')
  const minutes = now.getMinutes().toString().padStart(2, '0')
  return `${hours}:${minutes}`
}

// 检测消息中的风险等级
export function detectRiskLevel(message) {
  // 简单的风险检测逻辑
  const highRiskKeywords = ['转账', '汇款', '验证码', '银行卡', '投资', '高收益']
  const mediumRiskKeywords = ['保健品', '养生', '养老', '股票']

  let riskLevel = 0
  
  if (highRiskKeywords.some(keyword => message.includes(keyword))) {
    riskLevel = 2
  } else if (mediumRiskKeywords.some(keyword => message.includes(keyword))) {
    riskLevel = 1
  }

  return riskLevel
}