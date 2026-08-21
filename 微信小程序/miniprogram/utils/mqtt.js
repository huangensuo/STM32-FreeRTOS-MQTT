/**
 * mqtt.js
 * 微信小程序 MQTT over WebSocket 最小化实现
 * 用于直连 OneNET MQTT Broker 收发物模型数据
 */
var crypto = require('./crypto-helper.js');

var MQTT_CONFIG = {
  // MQTT over WebSocket 直连方式仅作为备用方案；默认小程序通过云函数访问 OneNET。
  URL: 'wss://your_product_id.mqttstls.acc.cmcconenet.cn:8883/mqtt',
  PRODUCT_ID: 'your_product_id',
  DEVICE_NAME: 'stm32_env_monitor',
  DEVICE_KEY: '',
  // 订阅主题（接收设备上报数据）
  SUB_TOPIC: '$sys/your_product_id/stm32_env_monitor/thing/property/post',
  // 发布主题（下发控制指令）
  PUB_TOPIC: '$sys/your_product_id/stm32_env_monitor/thing/property/set'
};

/**
 * 生成 MQTT 登录密码（version=2018-10-31 + HMAC-SHA1 + device_key）
 */
function generateMqttPassword() {
  if (!MQTT_CONFIG.DEVICE_KEY) {
    throw new Error('未配置MQTT设备密钥，当前小程序默认通过云函数访问OneNET');
  }
  var keyBytes = crypto.base64Decode(MQTT_CONFIG.DEVICE_KEY);
  var et = Math.floor(Date.now() / 1000) + 86400 * 365 * 10;
  var res = 'products/' + MQTT_CONFIG.PRODUCT_ID + '/devices/' + MQTT_CONFIG.DEVICE_NAME;
  var signStr = et + '\nsha1\n' + res + '\n2018-10-31';
  var signBytes = crypto.hmacSha1(keyBytes, signStr);
  var sign = crypto.base64Encode(signBytes);
  var signEncoded = sign.replace(/\+/g, '%2B').replace(/\//g, '%2F').replace(/=/g, '%3D');
  var resEncoded = encodeURIComponent(res);
  return 'version=2018-10-31&res=' + resEncoded + '&et=' + et + '&method=sha1&sign=' + signEncoded;
}

/**
 * 构建 MQTT CONNECT 报文
 */
function buildConnectPacket(clientId, username, password) {
  var payload = [];
  // 可变头: 协议名 "MQTT" + 协议级别 4
  var varHeader = [0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, 0x04];
  // 连接标志: username=1, password=1, cleanSession=1
  varHeader.push(0xC2);
  // Keep Alive: 120 秒
  varHeader.push(0x00, 0x78);

  // Payload: Client ID
  var clientIdBytes = strToUtf8(clientId);
  payload.push((clientIdBytes.length >> 8) & 0xFF, clientIdBytes.length & 0xFF);
  payload = payload.concat(clientIdBytes);

  // Username
  if (username) {
    var userBytes = strToUtf8(username);
    payload.push((userBytes.length >> 8) & 0xFF, userBytes.length & 0xFF);
    payload = payload.concat(userBytes);
  }

  // Password
  if (password) {
    var pwdBytes = strToUtf8(password);
    payload.push((pwdBytes.length >> 8) & 0xFF, pwdBytes.length & 0xFF);
    payload = payload.concat(pwdBytes);
  }

  // 固定头: 类型=CONNECT(1), 剩余长度
  var remaining = varHeader.length + payload.length;
  var fixedHeader = [0x10].concat(encodeRemainingLength(remaining));

  return new Uint8Array(fixedHeader.concat(varHeader).concat(payload)).buffer;
}

/**
 * 构建 MQTT SUBSCRIBE 报文
 */
function buildSubscribePacket(packetId, topic, qos) {
  qos = qos || 0;
  var topicBytes = strToUtf8(topic);
  var payload = [];
  payload.push((packetId >> 8) & 0xFF, packetId & 0xFF);
  payload.push((topicBytes.length >> 8) & 0xFF, topicBytes.length & 0xFF);
  payload = payload.concat(topicBytes);
  payload.push(qos);

  var fixedHeader = [0x82].concat(encodeRemainingLength(payload.length));
  return new Uint8Array(fixedHeader.concat(payload)).buffer;
}

/**
 * 构建 MQTT PUBLISH 报文
 */
function buildPublishPacket(topic, message, qos) {
  qos = qos || 0;
  var topicBytes = strToUtf8(topic);
  var msgBytes = strToUtf8(message);

  var payload = [];
  payload.push((topicBytes.length >> 8) & 0xFF, topicBytes.length & 0xFF);
  payload = payload.concat(topicBytes);
  // QoS > 0 需要 packetId
  if (qos > 0) {
    payload.push(0x00, 0x01); // packetId = 1
  }
  payload = payload.concat(msgBytes);

  var flags = (qos << 1) & 0x06;
  var fixedHeader = [0x30 | flags].concat(encodeRemainingLength(payload.length));
  return new Uint8Array(fixedHeader.concat(payload)).buffer;
}

/**
 * 构建 MQTT PINGREQ 报文
 */
function buildPingPacket() {
  return new Uint8Array([0xC0, 0x00]).buffer;
}

/**
 * 解析 MQTT 报文，提取 PUBLISH 消息
 */
function parseMqttMessage(buffer) {
  var data = new Uint8Array(buffer);
  if (data.length < 2) return null;
  var type = (data[0] >> 4) & 0x0F;
  if (type !== 3) return null; // 只处理 PUBLISH

  var pos = 1;
  var multiplier = 1;
  var remainingLen = 0;
  while (pos < data.length) {
    var byte = data[pos++];
    remainingLen += (byte & 0x7F) * multiplier;
    if ((byte & 0x80) === 0) break;
    multiplier *= 128;
  }

  var topicLen = (data[pos] << 8) | data[pos + 1];
  pos += 2;
  var topic = utf8ToStr(data.slice(pos, pos + topicLen));
  pos += topicLen;

  // 跳过 packetId (QoS > 0)
  var qos = (data[0] >> 1) & 0x03;
  if (qos > 0) pos += 2;

  var payload = utf8ToStr(data.slice(pos));
  return { topic: topic, payload: payload };
}

// ==================== 辅助函数 ====================

function strToUtf8(str) {
  var bytes = [];
  for (var i = 0; i < str.length; i++) {
    var code = str.charCodeAt(i);
    if (code < 0x80) {
      bytes.push(code);
    } else if (code < 0x800) {
      bytes.push(0xC0 | (code >> 6), 0x80 | (code & 0x3F));
    } else {
      bytes.push(0xE0 | (code >> 12), 0x80 | ((code >> 6) & 0x3F), 0x80 | (code & 0x3F));
    }
  }
  return bytes;
}

function utf8ToStr(bytes) {
  var str = '';
  var i = 0;
  while (i < bytes.length) {
    var b = bytes[i++];
    if (b < 0x80) {
      str += String.fromCharCode(b);
    } else if (b < 0xE0) {
      str += String.fromCharCode(((b & 0x1F) << 6) | (bytes[i++] & 0x3F));
    } else {
      str += String.fromCharCode(((b & 0x0F) << 12) | ((bytes[i++] & 0x3F) << 6) | (bytes[i++] & 0x3F));
    }
  }
  return str;
}

function encodeRemainingLength(len) {
  var bytes = [];
  while (len > 0) {
    var byte = len & 0x7F;
    len >>= 7;
    if (len > 0) byte |= 0x80;
    bytes.push(byte);
  }
  return bytes.length === 0 ? [0] : bytes;
}

// ==================== MQTT 连接管理 ====================

var socketTask = null;
var messageCallbacks = [];
var isConnected = false;
var packetId = 1;
var pingTimer = null;

function connect(callbacks) {
  if (callbacks) {
    messageCallbacks = [callbacks];
  }

  if (socketTask) {
    socketTask.close({ code: 1000 });
  }

  var password = generateMqttPassword();
  console.log('[MQTT] 连接中...');
  console.log('[MQTT] URL:', MQTT_CONFIG.URL);

  socketTask = wx.connectSocket({
    url: MQTT_CONFIG.URL,
    header: { 'Content-Type': 'application/json' },
    protocols: ['mqtt'],
    success: function () {
      console.log('[MQTT] Socket 创建成功');
    },
    fail: function (err) {
      console.error('[MQTT] Socket 创建失败:', err);
      messageCallbacks.forEach(function (cb) { if (cb.onError) cb.onError(err); });
    }
  });

  socketTask.onOpen(function () {
    console.log('[MQTT] WebSocket 已连接，发送 MQTT CONNECT...');
    var connectPkt = buildConnectPacket(
      MQTT_CONFIG.DEVICE_NAME,
      MQTT_CONFIG.PRODUCT_ID,
      password
    );
    socketTask.send({ data: connectPkt });
  });

  socketTask.onMessage(function (res) {
    var msg = parseMqttMessage(res.data);
    if (!msg) return;

    console.log('[MQTT] 收到消息 topic:', msg.topic);
    console.log('[MQTT] 收到消息 payload:', msg.payload);

    try {
      var data = JSON.parse(msg.payload);
      messageCallbacks.forEach(function (cb) {
        if (cb.onMessage) cb.onMessage(msg.topic, data);
      });
    } catch (e) {
      console.error('[MQTT] JSON 解析失败:', e);
    }
  });

  socketTask.onClose(function (res) {
    console.log('[MQTT] 连接关闭:', res.code, res.reason);
    isConnected = false;
    if (pingTimer) {
      clearInterval(pingTimer);
      pingTimer = null;
    }
    messageCallbacks.forEach(function (cb) { if (cb.onClose) cb.onClose(); });
  });

  socketTask.onError(function (err) {
    console.error('[MQTT] 错误:', err);
    messageCallbacks.forEach(function (cb) { if (cb.onError) cb.onError(err); });
  });

  // 等待 CONNACK 后订阅主题
  var connackHandler = function (res) {
    var data = new Uint8Array(res.data);
    if (data[0] === 0x20 && data[1] === 0x02 && data[3] === 0x00) {
      console.log('[MQTT] CONNACK 成功，订阅主题...');
      isConnected = true;
      socketTask.offMessage(connackHandler);

      // 订阅数据主题
      var subPkt = buildSubscribePacket(packetId++, MQTT_CONFIG.SUB_TOPIC, 0);
      socketTask.send({ data: subPkt });

      // 心跳
      pingTimer = setInterval(function () {
        if (socketTask && isConnected) {
          socketTask.send({ data: buildPingPacket() });
        }
      }, 60000);

      messageCallbacks.forEach(function (cb) { if (cb.onConnect) cb.onConnect(); });
    }
  };
  socketTask.onMessage(connackHandler);
}

function publish(topic, message) {
  return new Promise(function (resolve, reject) {
    if (!socketTask || !isConnected) {
      reject(new Error('MQTT 未连接'));
      return;
    }

    var payload = typeof message === 'string' ? message : JSON.stringify(message);
    console.log('[MQTT] 发布消息 topic:', topic, 'payload:', payload);

    var pkt = buildPublishPacket(topic, payload, 0);
    socketTask.send({ data: pkt });
    resolve();
  });
}

function disconnect() {
  if (pingTimer) {
    clearInterval(pingTimer);
    pingTimer = null;
  }
  if (socketTask) {
    socketTask.close({ code: 1000 });
    socketTask = null;
  }
  isConnected = false;
}

module.exports = {
  MQTT_CONFIG: MQTT_CONFIG,
  connect: connect,
  publish: publish,
  disconnect: disconnect,
  isConnected: function () { return isConnected; }
};
