/**
 * 直接触发微信插件登录流程
 * 不依赖 OpenClaw CLI
 */
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const require = createRequire(import.meta.url);

// 加载 openclaw plugin sdk
const openclawSdk = require('openclaw/plugin-sdk');

// 加载微信插件
const weixinPlugin = require('./node_modules/@tencent-weixin/openclaw-weixin/dist/index.js').default;

// 创建模拟的 OpenClaw API
const mockRuntime = {
  version: '2026.5.7'
};

const mockApi = {
  runtime: mockRuntime,
  registerChannel: ({ plugin }) => {
    console.log('Channel registered:', plugin.id);
  }
};

// 注册插件
weixinPlugin.register(mockApi);

console.log('Plugin registered. To trigger login, call:');
console.log('  weixinPlugin.login({ cfg, accountId, verbose: true, runtime: mockRuntime })');