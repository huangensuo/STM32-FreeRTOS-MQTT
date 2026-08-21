# Server 目录说明

当前项目的主链路已经改为 STM32 与 OneNET 直连：

```text
微信小程序 -> 云函数 -> OneNET 物模型服务调用 -> STM32
STM32 -> OneNET 物模型属性上报 -> 微信小程序查询
```

因此远程控制和数据查看不需要启动本目录下的 Python 服务。

本目录保留的是旧版/可选辅助代码：

- `main.py`：FastAPI + DeepSeek AI 实验服务，仍包含旧 EMQX MQTT 主题逻辑。
- `onenet_bridge.py`：旧版 EMQX 到 OneNET 的桥接脚本，当前直连方案不再需要。
- `requirements.txt` / `requirements_deploy.txt`：旧服务依赖。

如果后续要恢复 AI 后端，请先把其中的 MQTT 主题同步到 OneNET 物模型主题，避免再次走旧的 `device/stm32/*` 链路。
