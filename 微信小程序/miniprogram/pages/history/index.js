var { queryDeviceProperty, sendCommand } = require('../../utils/onenet.js');

Page({
  data: {
    count: 0,
    sourceText: '本地缓存',
    syncText: '正在同步Flash历史...',
    isSyncing: false
  },

  onShow: function () {
    this.syncFlashHistory();
  },

  syncFlashHistory: function () {
    var self = this;
    if (this.data.isSyncing) return;

    this.setData({
      syncText: '正在触发开发板上传Flash历史...',
      isSyncing: true
    });

    sendCommand('upload_history', { value: 1 }).then(function () {
      self.setData({ syncText: '已触发上传，正在等待OneNET属性刷新...' });
      self.queryFlashHistoryWithRetry(0);
    }).catch(function () {
      self.setData({ isSyncing: false });
      self.renderLocalHistory('同步失败，显示本地缓存');
    });
  },

  queryFlashHistoryWithRetry: function (attempt) {
    var self = this;
    var maxAttempts = 6;
    var delay = attempt === 0 ? 1800 : 1500;

    setTimeout(function () {
      queryDeviceProperty().then(function (result) {
        console.log('[History] OneNET属性查询结果:', JSON.stringify(result));
        var flashHistory = self.parseHistoryBatch(result.history_batch);
        if (flashHistory.length > 0) {
          self.setData({ isSyncing: false });
          self.renderHistory(flashHistory, 'Flash历史', '已同步Flash最近' + flashHistory.length + '条');
          return;
        }

        if (attempt + 1 < maxAttempts) {
          self.setData({
            syncText: '等待Flash历史上传中 ' + (attempt + 1) + '/' + maxAttempts
          });
          self.queryFlashHistoryWithRetry(attempt + 1);
          return;
        }

        self.setData({ isSyncing: false });
        self.renderLocalHistory('Flash属性为空：请检查history_batch物模型和设备上传日志');
      }).catch(function () {
        if (attempt + 1 < maxAttempts) {
          self.setData({
            syncText: '查询失败，正在重试 ' + (attempt + 1) + '/' + maxAttempts
          });
          self.queryFlashHistoryWithRetry(attempt + 1);
          return;
        }

        self.setData({ isSyncing: false });
        self.renderLocalHistory('同步失败，显示本地缓存');
      });
    }, delay);
  },

  renderLocalHistory: function (msg) {
    var app = getApp();
    var history = app.globalData.history || [];
    this.renderHistory(history, '本地缓存', msg || '');
  },

  renderHistory: function (history, sourceText, syncText) {
    this.setData({
      count: history.length,
      sourceText: sourceText,
      syncText: syncText || '已同步'
    });

    if (history.length >= 1) {
      // 延迟确保 Canvas 节点就绪
      var self = this;
      setTimeout(function () {
        self.drawChart('tempCanvas', history, 'temp', '#ff6b6b', '°C');
        self.drawChart('humCanvas', history, 'hum', '#74b9ff', '%');
        self.drawChart('smokeCanvas', history, 'smoke', '#ff8f00', '');
      }, 300);
    }
  },

  parseHistoryBatch: function (batch) {
    batch = normalizeHistoryBatch(batch);
    if (!batch || typeof batch !== 'string') return [];

    return batch.split(';').map(function (item) {
      var parts = item.split(',');
      if (parts.length < 4) return null;

      var seconds = parseInt(parts[0], 10);
      var temp = parseFloat(parts[1]);
      var hum = parseFloat(parts[2]);
      var smoke = parseInt(parts[3], 10);

      if (isNaN(seconds) || isNaN(temp) || isNaN(hum) || isNaN(smoke)) return null;

      return {
        time: formatUptime(seconds),
        temp: temp,
        hum: hum,
        smoke: smoke
      };
    }).filter(function (item) {
      return item !== null;
    });
  },

  drawChart: function (canvasId, data, key, color, unit) {
    var self = this;
    var query = wx.createSelectorQuery();
    query.select('#' + canvasId).fields({ node: true, size: true }).exec(function (res) {
      if (!res || !res[0] || !res[0].node) return;

      var canvas = res[0].node;
      var ctx = canvas.getContext('2d');
      var dpr = wx.getSystemInfoSync().pixelRatio;

      var width = res[0].width;
      var height = res[0].height;
      canvas.width = width * dpr;
      canvas.height = height * dpr;
      ctx.scale(dpr, dpr);

      // 清空
      ctx.clearRect(0, 0, width, height);

      // 边距
      var padding = { top: 20, right: 20, bottom: 40, left: 50 };
      var chartW = width - padding.left - padding.right;
      var chartH = height - padding.top - padding.bottom;

      // 背景
      ctx.fillStyle = '#fafafa';
      ctx.fillRect(0, 0, width, height);

      // 提取值和范围
      var values = [];
      for (var i = 0; i < data.length; i++) {
        values.push(data[i][key]);
      }
      var minVal = Math.floor(Math.min.apply(null, values) - 2);
      var maxVal = Math.ceil(Math.max.apply(null, values) + 2);
      if (minVal < 0) minVal = 0;
      var range = maxVal - minVal || 1;

      // 网格线
      ctx.strokeStyle = '#e0e0e0';
      ctx.lineWidth = 0.5;
      for (var g = 0; g <= 4; g++) {
        var gy = padding.top + (chartH / 4) * g;
        ctx.beginPath();
        ctx.moveTo(padding.left, gy);
        ctx.lineTo(width - padding.right, gy);
        ctx.stroke();

        // Y轴标签
      ctx.fillStyle = '#999';
      ctx.font = '10px sans-serif';
      ctx.textAlign = 'right';
      ctx.fillText((maxVal - (range / 4) * g).toFixed(1) + unit, padding.left - 8, gy + 4);
      }

      // X轴标签（最多5个，避免重叠）
      var xStep = Math.max(1, Math.floor(values.length / 5));
      var xSpan = Math.max(1, values.length - 1);
      ctx.fillStyle = '#999';
      ctx.font = '9px sans-serif';
      ctx.textAlign = 'center';
      for (var x = 0; x < values.length; x += xStep) {
        var gx = padding.left + (chartW / xSpan) * x;
        // 只显示时:分
        var t = data[x].time;
        if (t.length > 5) t = t.substring(0, 5);
        ctx.fillText(t, gx, height - padding.bottom + 18);
      }

      // 折线
      ctx.strokeStyle = color;
      ctx.lineWidth = 2;
      ctx.lineJoin = 'round';
      ctx.beginPath();
      for (var i = 0; i < values.length; i++) {
        var px = padding.left + (chartW / xSpan) * i;
        var py = padding.top + chartH - ((values[i] - minVal) / range) * chartH;
        if (i === 0) ctx.moveTo(px, py);
        else ctx.lineTo(px, py);
      }
      ctx.stroke();

      // 渐变填充
      var gradient = ctx.createLinearGradient(0, padding.top, 0, padding.top + chartH);
      gradient.addColorStop(0, color + '40');
      gradient.addColorStop(1, color + '05');
      ctx.lineTo(padding.left + (chartW / xSpan) * (values.length - 1), padding.top + chartH);
      ctx.lineTo(padding.left, padding.top + chartH);
      ctx.closePath();
      ctx.fillStyle = gradient;
      ctx.fill();

      // 数据点
      ctx.fillStyle = color;
      for (var i = 0; i < values.length; i++) {
        var dx = padding.left + (chartW / xSpan) * i;
        var dy = padding.top + chartH - ((values[i] - minVal) / range) * chartH;
        ctx.beginPath();
        ctx.arc(dx, dy, 3, 0, Math.PI * 2);
        ctx.fill();
      }
    });
  }
});

function formatUptime(seconds) {
  var minutes = Math.floor(seconds / 60);
  var hours = Math.floor(minutes / 60);
  var mins = minutes % 60;

  if (hours > 0) {
    return hours + 'h' + (mins < 10 ? '0' : '') + mins;
  }
  return minutes + 'm';
}

function normalizeHistoryBatch(batch) {
  var val = batch;
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

  if (typeof val !== 'string') return '';

  val = val.trim();
  if ((val.charAt(0) === '"' && val.charAt(val.length - 1) === '"') ||
      (val.charAt(0) === "'" && val.charAt(val.length - 1) === "'")) {
    try {
      var parsed = JSON.parse(val);
      if (typeof parsed === 'string') val = parsed;
    } catch (e) {}
  }

  return val;
}
