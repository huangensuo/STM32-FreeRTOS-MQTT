/**
 * @file    index.js
 * @brief   智能环境监测小程序首页逻辑
 * @details 实时展示传感器数据、本地AI分析结果，支持远程控制指令下发
 * @author  派大星
 * @date    2026-06
 */

// 导入OneNET API封装模块
var { queryDeviceProperty, sendCommand } = require('../../utils/onenet.js');

Page({
  /**
   * 页面数据绑定
   */
  data: {
    temp: '--',          // 温度显示值（°C）
    hum: '--',           // 湿度显示值（%）
    flame: false,        // 火焰状态（false=安全, true=检测到火焰）
    smoke: false,        // 烟雾状态（false=安全, true=检测到烟雾）
    smokeValue: 0,       // 烟雾ADC数值（OneNET smoke_adc: 0~4095）
    smokeStatus: 0,      // 烟雾/震动状态值（OneNET shock: 0=正常, 1=异常）
    state: 0,            // 系统状态（0=正常, 1=预警, 2=危险）
    stateText: '正常',
    stateClass: 'ok',
    isConnected: false,  // 连接状态标志
    aiAnalysis: '正在分析...',  // AI分析结果文本
    safetyLevel: '安全',        // 安全等级（安全/注意/预警/危险）
    safetyClass: 'ok',
    refreshTime: '--:--:--',   // 数据刷新时间
    commandSending: false      // 控制指令发送中
  },

  /**
   * 数据轮询定时器ID
   */
  intervalId: null,

  /**
   * 页面加载时触发
   * 初始化数据并开始轮询
   */
  onLoad: function () {
    this.refreshData();  // 立即刷新一次数据
    this.startPolling(); // 启动数据轮询
  },

  /**
   * 页面显示时触发
   * 重新启动轮询（页面从后台返回时）
   */
  onShow: function () {
    this.startPolling();
  },

  /**
   * 页面隐藏时触发
   * 停止轮询，节省资源
   */
  onHide: function () {
    this.stopPolling();
  },

  /**
   * 页面卸载时触发
   * 清理定时器
   */
  onUnload: function () {
    this.stopPolling();
  },

  /**
   * 启动数据轮询
   * 设置5秒间隔自动刷新数据
   */
  startPolling: function () {
    // 清除已有定时器，避免重复
    if (this.intervalId) clearInterval(this.intervalId);
    // 设置新的轮询定时器（5秒间隔）
    this.intervalId = setInterval(this.refreshData.bind(this), 5000);
  },

  /**
   * 停止数据轮询
   * 清除定时器，释放资源
   */
  stopPolling: function () {
    if (this.intervalId) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
  },

  /**
   * 刷新传感器数据
   * 通过OneNET API获取设备最新属性数据
   */
  refreshData: function () {
    var self = this;
    if (this.data.commandSending) {
      wx.showToast({ title: '指令发送中', icon: 'none' });
      return;
    }
    // 调用OneNET API查询设备属性
    queryDeviceProperty().then(function (result) {
      console.log('获取到的数据:', JSON.stringify(result));

      var temp = result.temp !== undefined ? result.temp : self.data.temp;
      var hum = result.hum !== undefined ? result.hum : self.data.hum;
      var flame = result.flame === 1 || result.flame === true || result.flame === '1';
      var smokeStatus = parseInt(result.shock, 10);
      var smokeVal = parseInt(result.smoke_adc, 10);
      var state = parseInt(result.state, 10);

      if (isNaN(smokeVal)) smokeVal = self.data.smokeValue || 0;
      if (isNaN(smokeStatus)) smokeStatus = self.data.smokeStatus || 0;
      if (isNaN(state)) state = self.data.state || 0;

      var stateInfo = self.getStateInfo(state);
      var smoke = smokeStatus === 1 || self.deriveSmokeStatus(state, flame, temp, hum);

      self.setData({
        isConnected: true,
        temp: temp,
        hum: hum,
        flame: flame,
        smokeValue: smokeVal,
        smokeStatus: smokeStatus,
        smoke: smoke,
        state: state,
        stateText: stateInfo.text,
        stateClass: stateInfo.className
      });

      // 将数据存入全局历史记录（用于图表展示）
      var app = getApp();
      if (!app.globalData.history) app.globalData.history = [];
      var now = new Date();
      var h = now.getHours(), m = now.getMinutes();
      var timeStr = (h < 10 ? '0' : '') + h + ':' + (m < 10 ? '0' : '') + m;
      app.globalData.history.push({
        time: timeStr,
        temp: parseFloat(result.temp) || 0,
        hum: parseFloat(result.hum) || 0,
        smoke: parseInt(result.smoke_adc, 10) || 0
      });
      // 限制历史记录最多100条
      if (app.globalData.history.length > 100) {
        app.globalData.history.shift();
      }

    }).catch(function (err) {
      // 获取数据失败，更新连接状态
      console.error('获取数据失败:', err);
      var msg = (err && (err.message || err.errMsg || err.error)) || '请检查云函数 onenetProxy 和 OneNET 密钥配置';
      self.setData({
        isConnected: false,
        aiAnalysis: '数据获取失败：' + msg,
        safetyLevel: '离线',
        safetyClass: 'danger',
        stateText: '离线',
        stateClass: 'danger'
      });
    }).then(function () {
      // 无论成功与否，都更新时间和执行本地AI分析
      self.updateTime();
      self.performAIAnalysis();
    });
  },

  /**
   * 更新显示时间
   * 格式化当前时间为 HH:MM:SS 格式
   */
  updateTime: function () {
    var now = new Date();
    var h = now.getHours().toString().padStart(2, '0');   // 小时（补零）
    var m = now.getMinutes().toString().padStart(2, '0'); // 分钟（补零）
    var s = now.getSeconds().toString().padStart(2, '0'); // 秒（补零）
    this.setData({ refreshTime: h + ':' + m + ':' + s });
  },

  getStateInfo: function (state) {
    if (state === 2) return { text: '危险', className: 'danger' };
    if (state === 1) return { text: '预警', className: 'warn' };
    return { text: '正常', className: 'ok' };
  },

  deriveSmokeStatus: function (state, flame, temp, hum) {
    var tempNum = parseFloat(temp);
    var humNum = parseFloat(hum);
    var tempHumWarning = (!isNaN(tempNum) && tempNum >= 35) || (!isNaN(humNum) && humNum >= 80);

    // 固件使用“MQ2预热基准线 + 偏移量”动态判定。前端没有基准线时，
    // 以设备状态为准：危险且非火焰基本就是烟雾危险；预警且非温湿度异常则视为烟雾偏高。
    if (state === 2 && !flame) return true;
    if (state === 1 && !flame && !tempHumWarning) return true;
    return false;
  },

  /**
   * 执行本地AI分析
   * 根据传感器数据进行规则引擎分析，生成环境评估报告
   */
  performAIAnalysis: function () {
    // 获取当前传感器数据
    var temp = this.data.temp, hum = this.data.hum;
    var flame = this.data.flame, smoke = this.data.smoke, state = this.data.state;
    var smokeValue = this.data.smokeValue;
    
    // 异常列表和安全等级
    var issues = [];
    var level = '安全';

    // 危险级别检测（火焰、烟雾、系统状态危险）
    if (flame) { issues.push('检测到火焰'); level = '危险'; }
    if (smoke) { issues.push('检测到烟雾异常（数值' + smokeValue + '）'); level = state === 2 ? '危险' : '预警'; }
    if (state === 2) { issues.push('系统状态危险'); level = '危险'; }

    // 温度异常检测
    var tempNum = parseFloat(temp);
    if (!isNaN(tempNum)) {
      if (tempNum >= 35) { issues.push('温度过高'); if (level !== '危险') level = '危险'; }
      else if (tempNum >= 30) { issues.push('温度偏高'); if (level !== '危险') level = '预警'; }
    }

    // 湿度异常检测
    var humNum = parseFloat(hum);
    if (!isNaN(humNum)) {
      if (humNum >= 80) { issues.push('湿度过高'); if (level !== '危险') level = '预警'; }
      else if (humNum <= 30) { issues.push('环境干燥'); if (level !== '危险' && level !== '预警') level = '注意'; }
    }

    // 生成分析结果文本
    var analysis = issues.length === 0
      ? '当前环境各项指标正常，系统运行良好。'
      : '检测到以下异常：' + issues.join('、') + '。' +
        (level === '危险' ? ' 请立即采取措施！' : (level === '预警' ? ' 请关注环境变化。' : ''));

    // 更新UI显示
    var safetyClass = 'ok';
    if (level === '危险') safetyClass = 'danger';
    else if (level === '预警') safetyClass = 'warn';
    else if (level === '注意') safetyClass = 'notice';

    this.setData({ aiAnalysis: analysis, safetyLevel: level, safetyClass: safetyClass });
  },

  /**
   * AI分析按钮点击处理
   * 重新执行本地AI分析并更新界面
   */
  onAiAnalyze: function () {
    this.performAIAnalysis();
    wx.showToast({ title: 'AI分析完成', icon: 'success' });
  },

  /**
   * 通过云函数代理MQTT下发控制指令
   * 云函数使用TCP直连MQTT Broker发布指令到设备
   * 
   * @param {Object} e - 事件对象，包含service和params数据
   */
  onSendCommand: function (e) {
    var self = this;

    if (this.data.commandSending) {
      wx.showToast({ title: '指令发送中', icon: 'none' });
      return;
    }

    // 获取触发事件的服务类型（relay/buzzer/led/rgb）
    var service = e.currentTarget.dataset.service;
    // 获取参数JSON字符串
    var paramsStr = e.currentTarget.dataset.params;
    var uiParams = {};

    // 解析参数JSON
    try {
      uiParams = JSON.parse(paramsStr);
    } catch (parseErr) {
      console.error('参数解析失败:', parseErr);
    }

    // 根据服务类型转换参数值
    var value;
    if (service === 'rgb') {
      // RGB颜色映射：red=0, green=1, blue=2, off=3
      var colorMap = { red: 0, green: 1, blue: 2, off: 3 };
      value = colorMap[uiParams.color] !== undefined ? colorMap[uiParams.color] : 3;
    } else if (service === 'relay' || service === 'buzzer' || service === 'led') {
      // 开关类型：1=开, 0=关
      value = uiParams.switch !== undefined ? uiParams.switch : 0;
    } else {
      // 其他服务默认值
      value = 1;
    }

    // 显示加载提示
    this.setData({ commandSending: true });
    wx.showLoading({ title: '发送中...' });

    function getCommandErrorMessage(err) {
      var raw = '';

      if (err && err.message) raw = err.message;
      else if (err && err.error) raw = err.error;
      else raw = JSON.stringify(err);

      if (service === 'rgb' && raw && raw.indexOf('int32 over range') !== -1) {
        return 'RGB蓝灯参数被OneNET拦截：当前发送 value=' + value +
          '。请到 OneNET 物模型里把 rgb 服务入参 value 的 int32 取值范围改为 0-3，步长 1。';
      }

      return raw || '指令调用失败';
    }

    sendCommand(service, { value: value, switch: value }).then(function (res) {
      // 隐藏加载提示
      wx.hideLoading();
      self.setData({ commandSending: false });
      console.log('[OneNET] 控制返回:', JSON.stringify(res));
      wx.showToast({ title: '指令已发送', icon: 'success' });
    }).catch(function (err) {
      wx.hideLoading();
      self.setData({ commandSending: false });
      console.error('[OneNET] 控制失败:', err);
      wx.showModal({
        title: '调用失败',
        content: getCommandErrorMessage(err),
        showCancel: false
      });
    });
  }
});
