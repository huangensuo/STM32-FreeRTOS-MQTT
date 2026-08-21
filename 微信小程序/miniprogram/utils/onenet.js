/**
 * onenet.js
 * OneNET Studio 物联网平台云函数代理封装
 * 鉴权在云函数 onenetProxy 内完成，前端不保存 Access Key
 *
 * 云函数动作:
 *   query   查询设备属性
 *   command 调用设备服务
 */

var ONENET_CONFIG = {
  PRODUCT_ID: 'your_product_id',
  DEVICE_NAME: 'stm32_env_monitor',
  PROXY_FUNCTION: 'onenetProxy'
};

function buildServiceErrorMessage(identifier, params, msg, code) {
  var value = params && params.value;
  if (identifier === 'rgb' && msg && msg.indexOf('int32 over range') !== -1) {
    return 'RGB蓝灯参数被OneNET拦截：当前发送 value=' + value +
      '。请到 OneNET 物模型里把 rgb 服务入参 value 的 int32 取值范围改为 0-3，步长 1。';
  }

  return '服务调用失败: code=' + code + ', msg=' + (msg || '');
}

function unwrapPropertyValue(rawValue) {
  var val = rawValue;
  var guard = 0;

  while (val && typeof val === 'object' && !Array.isArray(val) && guard < 4) {
    if (val.value !== undefined) {
      val = val.value;
    } else if (val.property_value !== undefined) {
      val = val.property_value;
    } else if (val.current_value !== undefined) {
      val = val.current_value;
    } else {
      break;
    }
    guard++;
  }

  if (typeof val === 'string') {
    var text = val.trim();
    if ((text.charAt(0) === '"' && text.charAt(text.length - 1) === '"') ||
        (text.charAt(0) === "'" && text.charAt(text.length - 1) === "'")) {
      try {
        var parsed = JSON.parse(text);
        if (typeof parsed === 'string') return parsed;
      } catch (e) {}
    }
  }

  return val;
}

function getPropertyKey(item) {
  if (!item || typeof item !== 'object') return '';
  return item.identifier || item.id || item.name || item.property_identifier || item.key || '';
}

function getRawPropertyValue(item) {
  if (!item || typeof item !== 'object') return undefined;
  if (item.value !== undefined) return item.value;
  if (item.property_value !== undefined) return item.property_value;
  if (item.current_value !== undefined) return item.current_value;
  if (item.latest_value !== undefined) return item.latest_value;
  return undefined;
}

function normalizePropertyValue(rawValue, item) {
  var val = unwrapPropertyValue(rawValue);
  var type = item && (item.data_type || item.dataType || item.type);

  if (type && typeof type === 'object') type = type.type;
  type = (type || '').toString().toLowerCase();

  if (type === 'float' || type === 'double') {
    var f = parseFloat(val);
    return isNaN(f) ? val : f;
  }
  if (type === 'int32' || type === 'int64' || type === 'bool' || type === 'boolean') {
    var n = parseInt(val, 10);
    return isNaN(n) ? val : n;
  }

  return val;
}

function addProperty(result, key, rawValue, item) {
  if (!key || rawValue === undefined) return;
  result[key] = normalizePropertyValue(rawValue, item);
}

function collectProperties(rawData, result, depth) {
  if (rawData === null || rawData === undefined || depth > 5) return;

  if (Array.isArray(rawData)) {
    rawData.forEach(function (item) {
      var key = getPropertyKey(item);
      var rawValue = getRawPropertyValue(item);
      if (key && rawValue !== undefined) {
        addProperty(result, key, rawValue, item);
      } else {
        collectProperties(item, result, depth + 1);
      }
    });
    return;
  }

  if (typeof rawData !== 'object') return;

  var directKey = getPropertyKey(rawData);
  var directValue = getRawPropertyValue(rawData);
  if (directKey && directValue !== undefined) {
    addProperty(result, directKey, directValue, rawData);
  }

  ['list', 'properties', 'params', 'items', 'data'].forEach(function (name) {
    if (rawData[name] !== undefined) {
      collectProperties(rawData[name], result, depth + 1);
    }
  });

  Object.keys(rawData).forEach(function (key) {
    if (key === 'code' || key === 'msg' || key === 'message' || key === 'desc' ||
        key === 'request_id' || key === 'success' || key === 'data' ||
        key === 'list' || key === 'properties' || key === 'params' || key === 'items') {
      return;
    }

    var prop = rawData[key];
    if (prop && typeof prop === 'object') {
      var rawValue = getRawPropertyValue(prop);
      if (rawValue !== undefined) {
        addProperty(result, key, rawValue, prop);
      } else {
        collectProperties(prop, result, depth + 1);
      }
    } else if (prop !== undefined) {
      addProperty(result, key, prop, null);
    }
  });
}

function parseDevicePropertyResponse(data) {
  var rawData = data || {};
  var result = {};

  collectProperties(rawData, result, 0);

  return result;
}

function generateToken() {
  // 鉴权已迁移到云函数，前端不再生成或打印OneNET Token。
  return '';
}

/**
 * 查询设备最新属性数据
 *
 * GET /thingmodel/query-device-property?product_id={pid}&device_name={name}
 */
function queryDeviceProperty() {
  return new Promise(function (resolve, reject) {
    wx.cloud.callFunction({
      name: ONENET_CONFIG.PROXY_FUNCTION,
      data: { action: 'query' },
      success: function (res) {
        var result = res.result || {};
        if (result.success === false) {
          reject(new Error(result.error || '云函数查询失败'));
          return;
        }
        resolve(parseDevicePropertyResponse(result.data || {}));
      },
      fail: reject
    });
  });
}

/**
 * 调用设备服务（用于控制只读属性场景下的指令下发）
 *
 * POST /thingmodel/call-service
 * Body: { product_id, device_name, identifier, params }
 */
function callService(identifier, params) {
  return new Promise(function (resolve, reject) {
    wx.cloud.callFunction({
      name: ONENET_CONFIG.PROXY_FUNCTION,
      data: {
        action: 'command',
        identifier: identifier,
        value: params && params.value
      },
      success: function (res) {
        var result = res.result || {};
        console.log('[OneNET] 云函数服务调用响应:', JSON.stringify(result, null, 2));
        if (result.success) {
          resolve(result);
        } else {
          reject(new Error(result.error || '云函数服务调用失败'));
        }
      },
      fail: reject
    });
  });
}

/**
 * 下发属性设置命令（仅适用于读写属性）
 *
 * POST /thingmodel/set-device-property
 * Body: { product_id, device_name, params: { identifier: value } }
 */
function sendProperty(identifier, value) {
  return callService(identifier, { value: value });
}

/**
 * 发送控制命令（兼容 UI 层接口）
 * 当前 OneNET 物模型中 relay/buzzer/led/rgb 是服务，统一走服务调用。
 */
function sendCommand(service, params) {
  // 构建服务参数
  var serviceParams = {};

  if (service === 'rgb') {
    var colorMap = { red: 0, green: 1, blue: 2, off: 3 };
    if (params.value !== undefined) {
      serviceParams.value = params.value;
    } else {
      serviceParams.value = colorMap[params.color] !== undefined ? colorMap[params.color] : 3;
    }
  } else if (service === 'relay' || service === 'buzzer' || service === 'led') {
    serviceParams.value = params.switch !== undefined ? params.switch : 0;
  } else if (service === 'ai_analyze' || service === 'get_status') {
    serviceParams.value = 1;
  } else {
    serviceParams = params;
  }

  // 优先尝试服务调用
  return callService(service, serviceParams);
}

module.exports = {
  ONENET_CONFIG: ONENET_CONFIG,
  queryDeviceProperty: queryDeviceProperty,
  sendCommand: sendCommand,
  sendProperty: sendProperty,
  generateToken: generateToken
};
