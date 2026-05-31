/**
 * 启动微信插件的脚本
 * 模拟 OpenClaw host，触发登录流程显示二维码
 */
const path = require('path');
const fs = require('fs');

// 设置环境变量
process.env.DISABLE_HOST_VERSION_CHECK = '1';
process.env.GATEWAY_BASE_URL = 'http://127.0.0.1:8080';
process.env.OPENCLAW_STATE_DIR = path.join(__dirname, '.openclaw-state');

async function main() {
  const mockRuntime = { version: '2026.5.7' };

  const weixinPlugin = require('./node_modules/@tencent-weixin/openclaw-weixin/dist/index.js').default;

  let loginCalled = false;

  const mockApi = {
    runtime: mockRuntime,
    registerChannel: ({ plugin }) => {
      console.log('[MockHost] Channel registered:', plugin.id);

      // 直接调用 login
      if (plugin.login && !loginCalled) {
        loginCalled = true;
        console.log('[MockHost] Calling plugin.login...');
        plugin.login({
          cfg: { channels: { 'openclaw-weixin': { accounts: {} } } },
          accountId: 'placeholder-im-bot',
          verbose: true,
          runtime: mockRuntime,
          log: (msg) => console.log('[Plugin]', msg)
        }).then(() => {
          console.log('[MockHost] Login completed');
        }).catch(err => {
          console.log('[MockHost] Login error:', err.message);
        });
      }
    }
  };

  weixinPlugin.register(mockApi);
  console.log('[MockHost] Plugin registered, waiting...');

  // 保持进程运行
  setTimeout(() => {
    console.log('[MockHost] Timeout, exiting');
    process.exit(0);
  }, 60000);
}

main().catch(err => {
  console.error('[MockHost] Error:', err);
  process.exit(1);
});