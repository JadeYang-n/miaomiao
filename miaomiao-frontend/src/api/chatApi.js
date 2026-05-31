// 与后端的通信
const API_BASE_URL = '/ai'

export function chatWithSSE(memoryId, message, onMessage, onError, onClose) {
  const url = `${API_BASE_URL}/chat?memoryId=${memoryId}&message=${encodeURIComponent(message)}`
  const eventSource = new EventSource(url)
  let messageReceived = false

  eventSource.onmessage = (event) => {
    messageReceived = true
    onMessage(event.data)
  }

  eventSource.onerror = (error) => {
    if (messageReceived) {
      eventSource.close()
      onClose()
      return
    }
    console.error('SSE 错误:', error)
    onError(error)
    eventSource.close()
  }

  eventSource.onclose = () => {
    onClose()
  }

  return eventSource
}

// 模拟API响应（用于开发测试）
export function mockChatResponse(message) {
  return new Promise((resolve) => {
    setTimeout(() => {
      resolve({ content: `🐱 你好！我是苗苗。`, riskLevel: 0 })
    }, 1000)
  })
}