App({
  onLaunch: function () {
    // 初始化云开发环境（上传 GitHub 前请替换为自己的云开发环境 ID）
    try {
      if (wx && wx.cloud) {
        wx.cloud.init({
          env: 'your_cloud_env_id',
          traceUser: true
        });
        console.log('云环境初始化成功');
      }
    } catch (e) {
      console.warn('云环境初始化失败，指令功能不可用:', e);
    }
    console.log('环境监测小程序启动');
  },
  
  globalData: {
    onenetConfig: {
      productId: 'your_product_id',
      deviceName: 'stm32_env_monitor'
    }
  }
});
