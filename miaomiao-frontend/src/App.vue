<template>
  <div class="app">
    <!-- 头部标题 -->
    <div class="app-header">
      <div class="header-content">
        <PixelCat :mood="catMood" :size="48" class="header-cat" />
        <div class="header-titles">
          <h1 class="app-title">老年人反诈助手苗苗</h1>
          <div class="app-subtitle">您的防诈骗小助手</div>
        </div>
        <button
          class="speak-toggle"
          :class="{ active: speakAiResponse }"
          @click="speakAiResponse = !speakAiResponse"
          :title="speakAiResponse ? '关闭语音播报' : '开启语音播报'"
        >
          {{ speakAiResponse ? '🔊' : '🔇' }}
        </button>
      </div>
    </div>

    <!-- 聊天区域 -->
    <div class="chat-container">
      <!-- 消息列表 -->
      <div class="messages-container" ref="messagesContainer">
        <div v-if="messages.length === 0" class="welcome-message">
          <div class="welcome-content">
            <PixelCat :mood="catMood" :size="80" class="welcome-cat" />
            <h2>欢迎使用 老年人反诈助手苗苗</h2>
            <p>我是您的防诈骗小助手，会帮您识别各类骗局</p>
            <ul>
              <li>保健品骗局</li>
              <li>炒股骗局</li>
              <li>电信诈骗</li>
              <li>养老诈骗</li>
            </ul>
            <p>请随时和我聊天，我会保护您的财产安全！</p>
          </div>
        </div>

        <!-- 历史消息 -->
        <ChatMessage
          v-for="message in messages"
          :key="message.id"
          :message="message.content"
          :is-user="message.isUser"
          :timestamp="message.timestamp"
          :risk-level="message.riskLevel"
          :cat-mood="message.catMood || 'neutral'"
        />

        <!-- AI 正在回复的消息 -->
        <div v-if="isAiTyping" class="chat-message ai-message">
          <div class="message-avatar">
            <PixelCat :mood="catMood" :size="40" />
          </div>
          <div class="message-content">
            <div class="message-bubble">
              <div class="ai-typing-content">
                <div class="ai-response-text message-markdown" v-html="currentAiResponseRendered"></div>
                <LoadingDots v-if="isStreaming" />
              </div>
            </div>
          </div>
        </div>
      </div>

      <!-- 输入框 -->
      <ChatInput
        :disabled="isAiTyping"
        @send-message="sendMessage"
        placeholder="请输入您的问题或分享您的经历..."
      />
    </div>

    <!-- 连接状态提示 -->
    <div v-if="connectionError" class="connection-error">
      <div class="error-content">
        <span class="error-icon">⚠️</span>
        <span>连接服务器失败，请检查后端服务是否启动</span>
      </div>
    </div>

    <!-- 风险预警提示 -->
    <div v-if="showRiskAlert" class="risk-alert" :class="`risk-level-${currentRiskLevel}`">
      <div class="alert-content">
        <span class="alert-icon">🚨</span>
        <span>{{ riskAlertMessage }}</span>
        <button class="alert-button" @click="showRiskAlert = false">关闭</button>
      </div>
    </div>
  </div>
</template>

<script>
import ChatMessage from './components/ChatMessage.vue'
import ChatInput from './components/ChatInput.vue'
import LoadingDots from './components/LoadingDots.vue'
import PixelCat from './components/PixelCat.vue'
import { chatWithSSE } from './api/chatApi.js'
import { generateMemoryId } from './utils/index.js'
import { marked } from 'marked'
import { speak, stopSpeaking, preloadVoices } from './utils/voice.js'

export default {
  name: 'App',
  components: {
    ChatMessage,
    ChatInput,
    LoadingDots,
    PixelCat
  },
  data() {
    return {
      messages: [],
      memoryId: null,
      isAiTyping: false,
      isStreaming: false,
      currentAiResponse: '',
      currentEventSource: null,
      connectionError: false,
      showRiskAlert: false,
      currentRiskLevel: 0,
      riskAlertMessage: '',
      catMood: 'neutral',
      speakAiResponse: false
    }
  },
  computed: {
    currentAiResponseRendered() {
      if (!this.currentAiResponse) return ''
      // 配置marked选项
      marked.setOptions({
        breaks: true, // 支持换行
        gfm: true, // 支持GitHub风格的Markdown
        sanitize: false, // 不过滤HTML
        highlight: function(code, lang) {
          return code
        }
      })
      return marked(this.currentAiResponse)
    }
  },
  methods: {
    sendMessage(message) {
      // 添加用户消息
      this.addMessage(message, true)
      
      // 开始AI回复
      this.startAiResponse(message)
    },
    
    addMessage(content, isUser = false, riskLevel = 0, catMood = 'neutral') {
      const message = {
        id: Date.now() + Math.random(),
        content,
        isUser,
        riskLevel,
        catMood,
        timestamp: new Date()
      }
      this.messages.push(message)
      this.scrollToBottom()

      // 如果有风险，显示预警
      if (riskLevel > 0 && !isUser) {
        this.showRiskAlert = true
        this.currentRiskLevel = riskLevel
        this.riskAlertMessage = riskLevel === 1 ? '⚠️ 注意：可能存在风险' : '🚨 警告：高风险骗局'
        this.catMood = riskLevel >= 2 ? 'warning' : 'alert'
      } else {
        this.catMood = 'neutral'
      }
    },

    startAiResponse(userMessage) {
      this.isAiTyping = true
      this.isStreaming = true
      this.currentAiResponse = ''
      this.connectionError = false
      this.catMood = 'happy' // 正在思考时开心的表情

      // 使用 SSE 连接后端
      this.currentEventSource = chatWithSSE(
        this.memoryId,
        userMessage,
        (data) => this.handleAiMessage(data),
        (error) => this.handleAiError(error),
        () => this.handleAiClose()
      )
    },

    handleAiMessage(data) {
      // AI返回的是纯文本，直接追加
      this.currentAiResponse += data
      this.scrollToBottom()
    },
    
    handleAiError(error) {
      console.error('AI 回复出错:', error)
      this.connectionError = true
      this.finishAiResponse()
      
      // 5秒后自动隐藏错误提示
      setTimeout(() => {
        this.connectionError = false
      }, 5000)
    },
    
    handleAiClose() {
      this.finishAiResponse()
    },
    
    finishAiResponse() {
      this.isStreaming = false

      // 保存当前的catMood用于消息
      const finalMood = this.catMood

      // 如果有内容，添加到消息列表
      if (this.currentAiResponse.trim()) {
        this.addMessage(this.currentAiResponse.trim(), false, this.currentRiskLevel, finalMood)

        // 语音播报 AI 回复（高风险或用户开启时）
        if (this.speakAiResponse || this.currentRiskLevel > 0) {
          // 提取纯文本（去掉 Markdown 格式）
          const plainText = this.currentAiResponse.replace(/[#*`_\[\]]/g, '').trim()
          speak(plainText)
        }
      }

      // 重置状态
      this.isAiTyping = false
      this.currentAiResponse = ''
      this.currentRiskLevel = 0
      this.catMood = 'neutral'

      // 重置连接错误状态
      this.connectionError = false

      // 关闭连接
      if (this.currentEventSource) {
        this.currentEventSource.close()
        this.currentEventSource = null
      }
    },
    
    scrollToBottom() {
      this.$nextTick(() => {
        const container = this.$refs.messagesContainer
        if (container) {
          container.scrollTop = container.scrollHeight
        }
      })
    },
    
    initializeChat() {
      this.memoryId = generateMemoryId()
      console.log('聊天室ID:', this.memoryId)
    }
  },
  
  mounted() {
    this.initializeChat()
    preloadVoices()
  },
  
  beforeUnmount() {
    // 组件销毁前关闭连接
    if (this.currentEventSource) {
      this.currentEventSource.close()
    }
  }
}
</script>

<style scoped>
.app {
  height: 100vh;
  display: flex;
  flex-direction: column;
  background-color: #f0f0f0;
}

.app-header {
  background-color: #4CAF50;
  padding: 12px 20px;
  border-bottom: 1px solid #e1e5e9;
  color: white;
}

.header-content {
  display: flex;
  align-items: center;
  gap: 12px;
}

.header-titles {
  display: flex;
  flex-direction: column;
}

.app-title {
  font-size: 22px;
  font-weight: bold;
  margin: 0;
}

.app-subtitle {
  font-size: 13px;
  margin-top: 2px;
  opacity: 0.9;
}

.speak-toggle {
  margin-left: auto;
  background: rgba(255,255,255,0.2);
  border: none;
  border-radius: 50%;
  width: 40px;
  height: 40px;
  font-size: 20px;
  cursor: pointer;
  transition: all 0.3s;
}

.speak-toggle:hover {
  background: rgba(255,255,255,0.3);
}

.speak-toggle.active {
  background: rgba(255,255,255,0.4);
}

.chat-container {
  flex: 1;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.messages-container {
  flex: 1;
  overflow-y: auto;
  padding: 20px 0;
}

.welcome-message {
  display: flex;
  justify-content: center;
  align-items: center;
  height: 100%;
  padding: 0 20px;
}

.welcome-content {
  text-align: center;
  max-width: 400px;
  color: #666;
}

.welcome-cat {
  margin: 0 auto 20px;
  display: block;
}

.welcome-icon {
  font-size: 48px;
  margin-bottom: 20px;
}

.welcome-content h2 {
  font-size: 20px;
  margin-bottom: 15px;
  color: #333;
}

.welcome-content p {
  margin-bottom: 10px;
  line-height: 1.5;
}

.welcome-content ul {
  text-align: left;
  margin: 15px 0;
}

.welcome-content li {
  margin-bottom: 5px;
}

/* 消息样式 */
.chat-message {
  display: flex;
  margin-bottom: 20px;
  padding: 0 20px;
}

.ai-message {
  justify-content: flex-start;
  flex-direction: row;
}

.user-message {
  justify-content: flex-end;
  flex-direction: row-reverse;
}

.message-avatar {
  display: flex;
  align-items: flex-start;
  margin: 0 10px;
}

.avatar {
  width: 40px;
  height: 40px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
  font-weight: bold;
  color: white;
}

.ai-avatar {
  background-color: #4CAF50;
}

.user-avatar {
  background-color: #2196F3;
}

.message-content {
  max-width: 70%;
  min-width: 100px;
}

.message-bubble {
  padding: 12px 16px;
  border-radius: 18px;
  position: relative;
  word-wrap: break-word;
  word-break: break-word;
  color: #333;
}

.ai-message .message-bubble {
  background-color: #f1f3f4;
  border-bottom-left-radius: 4px;
}

.user-message .message-bubble {
  background-color: #2196F3;
  color: white;
  border-bottom-right-radius: 4px;
}

.message-time {
  font-size: 12px;
  color: #999;
  margin-top: 5px;
  text-align: right;
}

.user-message .message-time {
  text-align: left;
}

/* 风险等级标记 */
.risk-level {
  display: inline-block;
  padding: 2px 8px;
  border-radius: 10px;
  font-size: 12px;
  margin-left: 10px;
}

.risk-level-1 {
  background-color: #FFC107;
  color: #333;
}

.risk-level-2 {
  background-color: #F44336;
  color: white;
}

.ai-typing-content {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.ai-response-text {
  font-size: 14px;
  line-height: 1.5;
}

/* AI实时回复的Markdown样式 */
.ai-response-text.message-markdown h1,
.ai-response-text.message-markdown h2,
.ai-response-text.message-markdown h3,
.ai-response-text.message-markdown h4,
.ai-response-text.message-markdown h5,
.ai-response-text.message-markdown h6 {
  margin: 0.5em 0;
  font-weight: bold;
}

.ai-response-text.message-markdown h1 { font-size: 1.5em; }
.ai-response-text.message-markdown h2 { font-size: 1.3em; }
.ai-response-text.message-markdown h3 { font-size: 1.2em; }
.ai-response-text.message-markdown h4 { font-size: 1.1em; }
.ai-response-text.message-markdown h5 { font-size: 1em; }
.ai-response-text.message-markdown h6 { font-size: 0.9em; }

.ai-response-text.message-markdown p {
  margin: 0.5em 0;
}

.ai-response-text.message-markdown ul,
.ai-response-text.message-markdown ol {
  margin: 0.5em 0;
  padding-left: 1.5em;
}

.ai-response-text.message-markdown li {
  margin: 0.2em 0;
}

.ai-response-text.message-markdown code {
  background-color: rgba(0, 0, 0, 0.1);
  padding: 0.2em 0.4em;
  border-radius: 3px;
  font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
  font-size: 0.9em;
}

.ai-response-text.message-markdown pre {
  background-color: rgba(0, 0, 0, 0.1);
  padding: 1em;
  border-radius: 5px;
  overflow-x: auto;
  margin: 0.5em 0;
}

.ai-response-text.message-markdown pre code {
  background-color: transparent;
  padding: 0;
  font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
  font-size: 0.9em;
}

.ai-response-text.message-markdown blockquote {
  border-left: 4px solid #ccc;
  padding-left: 1em;
  margin: 0.5em 0;
  font-style: italic;
  color: #666;
}

.ai-response-text.message-markdown a {
  color: #007bff;
  text-decoration: underline;
}

.ai-response-text.message-markdown table {
  border-collapse: collapse;
  width: 100%;
  margin: 0.5em 0;
}

.ai-response-text.message-markdown th,
.ai-response-text.message-markdown td {
  border: 1px solid #ddd;
  padding: 0.5em;
  text-align: left;
}

.ai-response-text.message-markdown th {
  background-color: #f2f2f2;
  font-weight: bold;
}

.ai-response-text.message-markdown hr {
  border: none;
  border-top: 1px solid #ddd;
  margin: 1em 0;
}

.connection-error {
  position: fixed;
  top: 20px;
  left: 50%;
  transform: translateX(-50%);
  background-color: #ff4444;
  color: white;
  padding: 10px 20px;
  border-radius: 5px;
  z-index: 1000;
  animation: slideDown 0.3s ease-out;
}

.error-content {
  display: flex;
  align-items: center;
  gap: 8px;
}

.error-icon {
  font-size: 16px;
}

.risk-alert {
  position: fixed;
  bottom: 80px;
  left: 50%;
  transform: translateX(-50%);
  padding: 12px 20px;
  border-radius: 5px;
  z-index: 1000;
  animation: slideUp 0.3s ease-out;
  display: flex;
  align-items: center;
  gap: 10px;
}

.risk-level-1 {
  background-color: #FFC107;
  color: #333;
}

.risk-level-2 {
  background-color: #F44336;
  color: white;
}

.alert-content {
  display: flex;
  align-items: center;
  gap: 10px;
}

.alert-icon {
  font-size: 18px;
}

.alert-button {
  background: none;
  border: none;
  cursor: pointer;
  font-size: 16px;
  margin-left: 10px;
}

@keyframes slideDown {
  from {
    transform: translateX(-50%) translateY(-100%);
    opacity: 0;
  }
  to {
    transform: translateX(-50%) translateY(0);
    opacity: 1;
  }
}

@keyframes slideUp {
  from {
    transform: translateX(-50%) translateY(100%);
    opacity: 0;
  }
  to {
    transform: translateX(-50%) translateY(0);
    opacity: 1;
  }
}

/* 滚动条样式 */
.messages-container::-webkit-scrollbar {
  width: 6px;
}

.messages-container::-webkit-scrollbar-track {
  background: #f1f1f1;
}

.messages-container::-webkit-scrollbar-thumb {
  background: #c1c1c1;
  border-radius: 3px;
}

.messages-container::-webkit-scrollbar-thumb:hover {
  background: #a8a8a8;
}

@media (max-width: 768px) {
  .app-header {
    padding: 15px;
  }
  
  .app-title {
    font-size: 20px;
  }
  
  .messages-container {
    padding: 15px 0;
  }
  
  .welcome-content {
    padding: 0 10px;
  }
  
  .message-content {
    max-width: 85%;
  }
  
  .chat-message {
    padding: 0 10px;
  }
}
</style>