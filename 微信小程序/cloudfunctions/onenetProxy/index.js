/**
 * @file    index.js
 * @brief   微信云函数：OneNET设备代理服务
 * @details 提供HTTP API查询设备属性和OneNET物模型服务调用功能
 * @author  派大星
 * @date    2026-06
 */

// 导入Node.js核心模块
const crypto = require('crypto');  // 加密模块，用于生成OneNET鉴权Token
const https = require('https');   // HTTPS模块，用于访问OneNET API

let LOCAL_CONFIG = {};
try {
  LOCAL_CONFIG = require('../../local-secrets/onenetProxy.js');
} catch (e) {
  LOCAL_CONFIG = {};
}

/**
 * 配置参数
 * 包含OneNET平台信息
 */
const CONFIG = {
  BASE_URL: process.env.ONENET_BASE_URL || LOCAL_CONFIG.ONENET_BASE_URL || 'https://iot-api.heclouds.com',
  PRODUCT_ID: process.env.ONENET_PRODUCT_ID || LOCAL_CONFIG.ONENET_PRODUCT_ID || 'your_product_id',
  DEVICE_NAME: process.env.ONENET_DEVICE_NAME || LOCAL_CONFIG.ONENET_DEVICE_NAME || 'stm32_env_monitor',
  ACCESS_KEY: process.env.ONENET_ACCESS_KEY || LOCAL_CONFIG.ONENET_ACCESS_KEY || '',
  USER_ID: process.env.ONENET_USER_ID || LOCAL_CONFIG.ONENET_USER_ID || 'your_onenet_user_id'
};

function decodeAccessKey(accessKey) {
  if (!accessKey) return null;
  if (/^[0-9a-fA-F]+$/.test(accessKey) && accessKey.length % 2 === 0) {
    return Buffer.from(accessKey, 'hex');
  }
  return Buffer.from(accessKey, 'base64');
}

function encodeTokenPart(value) {
  return encodeURIComponent(value).replace(/\(/g, '%28').replace(/\)/g, '%29');
}

function signToken(accessKey, method, res, version) {
  const key = decodeAccessKey(accessKey);
  if (!key || key.length === 0) {
    throw new Error('未配置 ONENET_ACCESS_KEY 云函数环境变量');
  }

  const et = Math.floor(Date.now() / 1000) + 86400 * 365 * 10;
  const signStr = et + '\n' + method + '\n' + res + '\n' + version;
  const hmac = crypto.createHmac(method, key);
  hmac.update(signStr);
  return 'version=' + version +
    '&res=' + encodeTokenPart(res) +
    '&et=' + et +
    '&method=' + method +
    '&sign=' + encodeTokenPart(hmac.digest('base64'));
}

/**
 * 生成OneNET平台鉴权Token
 * 使用HMAC-MD5算法生成访问API所需的Authorization令牌
 * 
 * @returns {string} 生成的鉴权Token字符串
 */
function generateToken() {
  return signToken(CONFIG.ACCESS_KEY, 'md5', 'userid/' + CONFIG.USER_ID, '2022-05-01');
}

function buildCandidateTokens() {
  const productRes = 'products/' + CONFIG.PRODUCT_ID;
  const deviceRes = productRes + '/devices/' + CONFIG.DEVICE_NAME;
  return [
    { name: 'product-sha1-2018', token: signToken(CONFIG.ACCESS_KEY, 'sha1', productRes, '2018-10-31') },
    { name: 'user-md5-2022', token: generateToken() },
    { name: 'device-sha1-2018', token: signToken(CONFIG.ACCESS_KEY, 'sha1', deviceRes, '2018-10-31') }
  ];
}

function isAuthError(res) {
  const text = JSON.stringify(res && res.data ? res.data : res || '');
  return text.indexOf('invalid authorization') !== -1 ||
         text.indexOf('authentication failed') !== -1 ||
         text.indexOf('鉴权') !== -1 ||
         text.indexOf('authorization') !== -1;
}

/**
 * 发送HTTPS GET请求
 * 
 * @param {string} url - 请求URL
 * @param {string} token - 鉴权Token
 * @returns {Promise<Object>} 包含状态码和响应数据的对象
 */
function httpGet(url, token) {
  return new Promise(function (resolve, reject) {
    // 解析URL
    var u = new URL(url);
    // 创建HTTPS请求
    https.request({
      hostname: u.hostname,       // 主机名
      port: 443,                  // HTTPS端口
      path: u.pathname + u.search, // 请求路径
      method: 'GET',              // HTTP方法
      timeout: 15000,             // 超时时间（15秒）
      headers: {                  // 请求头
        'Content-Type': 'application/json',
        'Authorization': token    // 鉴权Token
      }
    }, function (res) {
      var body = '';
      // 接收数据块
      res.on('data', function (c) { body += c; });
      // 数据接收完成
      res.on('end', function () {
        try {
          // 尝试解析JSON
          resolve({ code: res.statusCode, data: JSON.parse(body) });
        } catch (e) {
          // JSON解析失败，返回原始数据
          resolve({ code: res.statusCode, data: body });
        }
      });
    }).on('error', reject).end();  // 设置错误处理并发送请求
  });
}

/**
 * 发送HTTPS POST请求
 * 
 * @param {string} url - 请求URL
 * @param {string} token - 鉴权Token
 * @param {Object} body - 请求体
 * @returns {Promise<Object>} 包含状态码和响应数据的对象
 */
function httpPost(url, token, body) {
  return new Promise(function (resolve, reject) {
    var u = new URL(url);
    var data = JSON.stringify(body);

    var req = https.request({
      hostname: u.hostname,
      port: 443,
      path: u.pathname + u.search,
      method: 'POST',
      timeout: 15000,
      headers: {
        'Content-Type': 'application/json',
        'Authorization': token,
        'Content-Length': Buffer.byteLength(data)
      }
    }, function (res) {
      var resp = '';
      res.on('data', function (c) { resp += c; });
      res.on('end', function () {
        try {
          resolve({ code: res.statusCode, data: JSON.parse(resp) });
        } catch (e) {
          resolve({ code: res.statusCode, data: resp });
        }
      });
    });

    req.on('error', reject);
    req.write(data);
    req.end();
  });
}

/**
 * 校验OneNET服务和值
 *
 * @param {string} identifier - 服务标识符（relay/buzzer/led/rgb等）
 * @param {number|string} value - 服务参数值
 * @returns {boolean} 是否允许下发
 */
function isValidControlService(identifier, value) {
  if (identifier === 'relay' || identifier === 'buzzer' || identifier === 'led') {
    return value === 0 || value === 1 || value === '0' || value === '1';
  }
  if (identifier === 'rgb') {
    return value === 0 || value === 1 || value === 2 || value === 3 ||
           value === '0' || value === '1' || value === '2' || value === '3';
  }
  if (identifier === 'get_status' || identifier === 'ai_analyze' || identifier === 'upload_history') {
    return true;
  }
  if (identifier === 'temp_threshold' || identifier === 'hum_threshold') {
    var n = parseInt(value, 10);
    return !isNaN(n);
  }
  return false;
}

/**
 * 归一化OneNET服务参数
 *
 * @param {string} identifier - 服务标识符
 * @param {number|string} value - 原始服务参数值
 * @returns {number} 数字服务参数值
 */
function normalizeServiceValue(identifier, value) {
  var n = parseInt(value, 10);
  if (identifier === 'rgb') {
    if (n < 0 || n > 3) return 3;
    return n;
  }
  if (identifier === 'temp_threshold' || identifier === 'hum_threshold') {
    return n;
  }
  return n ? 1 : 0;
}

/**
 * 调用OneNET物模型服务
 *
 * @param {string} identifier - 服务标识符（relay/buzzer/led/rgb）
 * @param {number|string} value - 服务参数值
 * @returns {Promise<Object>} OneNET响应
 */
async function callDeviceService(identifier, value) {
  if (!isValidControlService(identifier, value)) {
    return { success: false, error: '不支持的服务或服务参数' };
  }

  var params = {};
  if (identifier === 'get_status' || identifier === 'ai_analyze' || identifier === 'upload_history') {
    params.value = 1;
  } else {
    params.value = normalizeServiceValue(identifier, value);
  }

  var body = {
    product_id: CONFIG.PRODUCT_ID,
    device_name: CONFIG.DEVICE_NAME,
    identifier: identifier,
    params: params
  };

  var url = CONFIG.BASE_URL + '/thingmodel/call-service';
  var candidates = buildCandidateTokens();
  var attempts = [];

  for (var i = 0; i < candidates.length; i++) {
    var res = await httpPost(url, candidates[i].token, body);
    var ok = res.code >= 200 && res.code < 300 &&
             res.data && (Number(res.data.code) === 0 || res.data.success === true);
    var attempt = { auth: candidates[i].name, api: 'iot-api/thingmodel/call-service', statusCode: res.code, data: res.data };
    attempts.push(attempt);

    if (ok) {
      return { success: true, data: res.data, request: body, api: attempt.api, auth: candidates[i].name, attempts: attempts };
    }

    if (!isAuthError(res)) {
      break;
    }
  }

  var last = attempts[attempts.length - 1] || {};
  var lastData = last.data || {};
  return {
    success: false,
    error: 'OneNET服务调用失败: ' + (lastData.msg || lastData.message || lastData.desc || ('HTTP ' + res.code)),
    data: lastData,
    request: body,
    statusCode: last.statusCode,
    api: 'iot-api/thingmodel/call-service',
    attempts: attempts
  };
}

/**
 * 云函数主入口
 * 根据action参数执行不同操作：query查询设备属性，command调用设备服务
 * 
 * @param {Object} event - 事件对象，包含action、identifier、value等参数
 * @param {Object} context - 云函数上下文
 * @returns {Object} 执行结果
 */
exports.main = async function (event, context) {
  try {
    // 查询设备属性
    if (event.action === 'query') {
      var url = CONFIG.BASE_URL + '/thingmodel/query-device-property?product_id=' +
                CONFIG.PRODUCT_ID + '&device_name=' + CONFIG.DEVICE_NAME;
      var attempts = [];
      var candidates = buildCandidateTokens();

      for (var i = 0; i < candidates.length; i++) {
        var res = await httpGet(url, candidates[i].token);  // 调用OneNET API
        attempts.push({ auth: candidates[i].name, statusCode: res.code, data: res.data });
        if (res.code >= 200 && res.code < 300 && res.data && Number(res.data.code) === 0) {
          return { success: true, data: res.data, auth: candidates[i].name, attempts: attempts };
        }
        if (!isAuthError(res)) {
          break;
        }
      }

      var last = attempts[attempts.length - 1] || {};
      var lastData = last.data || {};
      return {
        success: false,
        error: 'OneNET属性查询失败: ' + ((lastData && (lastData.msg || lastData.message || lastData.desc)) || ('HTTP ' + last.statusCode)),
        data: lastData,
        statusCode: last.statusCode,
        attempts: attempts
      };
    }

    // 通过OneNET物模型服务调用，由平台下发到STM32
    if (event.action === 'command') {
      var callResult = await callDeviceService(event.identifier, event.value);
      console.log('[proxy] OneNET服务调用:', JSON.stringify(callResult));
      return callResult;
    }

    // 未知操作类型
    return { success: false, error: '未知 action' };
  } catch (err) {
    console.error('[proxy] 错误:', err);
    return { success: false, error: err.message || String(err) };
  }
};
