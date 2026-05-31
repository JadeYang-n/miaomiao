<template>
  <div class="chat-input-container">
    <div class="input-wrapper">
      <button
        @click="toggleVoiceInput"
        :class="['voice-button', { listening: isListening }]"
        :title="isListening ? '停止录音' : '语音输入'"
      >
        <span class="voice-icon">{{ isListening ? '⏹' : '🎤' }}</span>
      </button>
      <input
        type="text"
        v-model="inputMessage"
        :disabled="disabled"
        @keyup.enter="handleSend"
        :placeholder="placeholder"
        class="chat-input"
      />
      <button
        @click="handleSend"
        :disabled="disabled || !inputMessage.trim()"
        class="send-button"
      >
        <span class="send-icon">➤</span>
      </button>
    </div>
    <div v-if="isListening" class="voice-hint">正在聆听，请说话...</div>
  </div>
</template>

<script>
import { startListening, stopListening, isSpeechRecognitionSupported } from '../utils/voice.js'

export default {
  name: 'ChatInput',
  props: {
    disabled: {
      type: Boolean,
      default: false
    },
    placeholder: {
      type: String,
      default: '请输入消息...'
    }
  },
  emits: ['send-message'],
  data() {
    return {
      inputMessage: '',
      isListening: false,
      voiceSupported: true
    }
  },
  mounted() {
    this.voiceSupported = isSpeechRecognitionSupported()
    if (!this.voiceSupported) {
      console.warn('浏览器不支持语音识别')
    }
  },
  methods: {
    handleSend() {
      if (!this.disabled && this.inputMessage.trim()) {
        this.$emit('send-message', this.inputMessage.trim())
        this.inputMessage = ''
      }
    },
    toggleVoiceInput() {
      if (this.disabled) return

      if (this.isListening) {
        stopListening()
        this.isListening = false
      } else {
        startListening(
          (transcript) => {
            this.inputMessage = transcript
            // 自动发送
            if (transcript.trim()) {
              this.handleSend()
            }
          },
          (error) => {
            console.error('语音识别错误:', error)
            this.isListening = false
          },
          () => {
            this.isListening = false
          }
        )
        this.isListening = true
      }
    }
  }
}
</script>

<style scoped>
.chat-input-container {
  padding: 15px;
  background-color: #fff;
  border-top: 1px solid #e1e5e9;
}

.input-wrapper {
  display: flex;
  align-items: center;
  gap: 10px;
  max-width: 800px;
  margin: 0 auto;
}

.chat-input {
  flex: 1;
  padding: 12px 16px;
  border: 1px solid #ddd;
  border-radius: 20px;
  font-size: 14px;
  outline: none;
  transition: border-color 0.3s;
}

.chat-input:focus {
  border-color: #4CAF50;
}

.chat-input:disabled {
  background-color: #f5f5f5;
  cursor: not-allowed;
}

.send-button {
  width: 40px;
  height: 40px;
  border: none;
  border-radius: 50%;
  background-color: #4CAF50;
  color: white;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: background-color 0.3s;
}

.send-button:hover:not(:disabled) {
  background-color: #45a049;
}

.send-button:disabled {
  background-color: #ccc;
  cursor: not-allowed;
}

.send-icon {
  font-size: 18px;
  font-weight: bold;
}

.voice-button {
  width: 40px;
  height: 40px;
  border: none;
  border-radius: 50%;
  background-color: #2196F3;
  color: white;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.3s;
  flex-shrink: 0;
}

.voice-button:hover:not(:disabled) {
  background-color: #1976D2;
}

.voice-button.listening {
  background-color: #F44336;
  animation: pulse 1.5s infinite;
}

.voice-icon {
  font-size: 18px;
}

.voice-hint {
  text-align: center;
  color: #F44336;
  font-size: 13px;
  margin-top: 8px;
  animation: blink 1s infinite;
}

@keyframes pulse {
  0% { transform: scale(1); }
  50% { transform: scale(1.1); }
  100% { transform: scale(1); }
}

@keyframes blink {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

/* 响应式设计 */
@media (max-width: 768px) {
  .chat-input-container {
    padding: 10px;
  }
  
  .chat-input {
    padding: 10px 14px;
  }
  
  .send-button {
    width: 36px;
    height: 36px;
  }
}
</style>