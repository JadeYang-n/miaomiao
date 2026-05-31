/**
 * 语音识别 - 使用浏览器 Web Speech API
 */

let recognition = null
let isListening = false

// 检查浏览器支持
const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition

export function isSpeechRecognitionSupported() {
  return !!SpeechRecognition
}

export function startListening(onResult, onError, onEnd) {
  if (!SpeechRecognition) {
    onError?.('浏览器不支持语音识别')
    return
  }

  if (isListening) {
    stopListening()
  }

  recognition = new SpeechRecognition()
  recognition.lang = 'zh-CN'
  recognition.continuous = false
  recognition.interimResults = false
  recognition.maxAlternatives = 1

  recognition.onresult = (event) => {
    const transcript = event.results[0][0].transcript
    onResult?.(transcript)
  }

  recognition.onerror = (event) => {
    if (event.error !== 'no-speech' && event.error !== 'aborted') {
      onError?.(event.error)
    }
  }

  recognition.onend = () => {
    isListening = false
    onEnd?.()
  }

  try {
    recognition.start()
    isListening = true
  } catch (e) {
    onError?.(e.message)
  }
}

export function stopListening() {
  if (recognition && isListening) {
    recognition.stop()
    isListening = false
  }
}

export function isCurrentlyListening() {
  return isListening
}

/**
 * 语音合成 - 使用浏览器自带 TTS
 */

function fallbackSpeak(text, lang = 'zh-CN') {
  if (!window.speechSynthesis) return false

  window.speechSynthesis.cancel()

  const utterance = new SpeechSynthesisUtterance(text.replace(/[#*`_\[\]]/g, ''))
  utterance.lang = lang
  utterance.rate = 1.4
  utterance.pitch = 1.8

  window.speechSynthesis.speak(utterance)
  return true
}

/**
 * 主语音播放函数：使用浏览器 TTS
 */
export function speak(text, lang = 'zh-CN') {
  fallbackSpeak(text, lang)
  return true
}

export function stopSpeaking() {
  if (window.speechSynthesis) {
    window.speechSynthesis.cancel()
  }
}

export function isSpeaking() {
  return window.speechSynthesis?.speaking || false
}

// 预加载语音列表
export function preloadVoices() {
  if (window.speechSynthesis) {
    window.speechSynthesis.getVoices()
    window.speechSynthesis.onvoiceschanged = () => {
      window.speechSynthesis.getVoices()
    }
  }
}