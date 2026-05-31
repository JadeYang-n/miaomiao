<template>
  <div class="chat-message" :class="isUser ? 'user-message' : 'ai-message'">
    <div class="message-avatar">
      <PixelCat v-if="!isUser" :mood="catMood" :size="40" />
      <div v-else class="avatar user-avatar">👤</div>
    </div>
    <div class="message-content">
      <div class="message-bubble">
        <div class="message-text" v-html="renderedMessage"></div>
        <div v-if="riskLevel > 0" class="risk-level" :class="`risk-level-${riskLevel}`">
          {{ riskLevel === 1 ? '注意' : '警告' }}
        </div>
      </div>
      <div class="message-time">{{ formatTime(timestamp) }}</div>
    </div>
  </div>
</template>

<script>
import { marked } from 'marked'
import PixelCat from './PixelCat.vue'

export default {
  name: 'ChatMessage',
  props: {
    message: {
      type: String,
      required: true
    },
    isUser: {
      type: Boolean,
      default: false
    },
    timestamp: {
      type: Date,
      default: () => new Date()
    },
    riskLevel: {
      type: Number,
      default: 0
    },
    catMood: {
      type: String,
      default: 'neutral'
    }
  },
  components: {
    PixelCat
  },
  computed: {
    renderedMessage() {
      if (this.isUser) {
        return this.message
      } else {
        // 对AI消息进行Markdown渲染
        marked.setOptions({
          breaks: true,
          gfm: true,
          sanitize: false
        })
        return marked(this.message)
      }
    }
  },
  methods: {
    formatTime(date) {
      const now = new Date(date)
      const hours = now.getHours().toString().padStart(2, '0')
      const minutes = now.getMinutes().toString().padStart(2, '0')
      return `${hours}:${minutes}`
    }
  }
}
</script>

<style scoped>
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
  display: flex;
  align-items: flex-start;
  gap: 10px;
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

.message-text {
  flex: 1;
  font-size: 14px;
  line-height: 1.5;
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
  align-self: flex-start;
}

.risk-level-1 {
  background-color: #FFC107;
  color: #333;
}

.risk-level-2 {
  background-color: #F44336;
  color: white;
}

/* 响应式设计 */
@media (max-width: 768px) {
  .message-content {
    max-width: 85%;
  }
  
  .chat-message {
    padding: 0 10px;
  }
}
</style>