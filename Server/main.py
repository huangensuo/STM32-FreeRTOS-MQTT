"""
STM32智能室内环境质量监测与安全预警系统 - 服务器端

功能：
1. MQTT接收STM32传感器数据
2. DeepSeek大模型智能环境分析
3. RESTful API供前端/移动端查询
4. 远程控制指令下发

作者：派大星
日期：2026-06

技术栈：
- Python 3.8+
- FastAPI（RESTful API框架）
- paho-mqtt（MQTT客户端）
- requests（HTTP请求）
- DeepSeek API（AI分析）

部署说明：
1. 安装依赖：pip install -r requirements.txt
2. 配置.env文件（MQTT、OneNET、DeepSeek参数）
3. 启动服务：python main.py
4. 访问API文档：http://localhost:8000/docs
"""

import os
import json
import time
import logging
import threading
import requests
import paho.mqtt.client as mqtt
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from dotenv import load_dotenv

# ============================================================================
# 初始化
# ============================================================================

# 加载环境变量配置
load_dotenv()

# 配置日志系统
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

# 创建FastAPI应用实例
app = FastAPI(
    title="Smart Home Environment Monitor",
    version="1.0.0",
    description="STM32智能室内环境质量监测与安全预警系统 - 服务器API"
)

# 添加CORS中间件，允许跨域请求
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],        # 允许所有来源（生产环境应限制具体域名）
    allow_credentials=True,      # 允许携带凭证
    allow_methods=["*"],         # 允许所有HTTP方法
    allow_headers=["*"],         # 允许所有请求头
)

# 挂载静态文件目录（用于Dashboard等前端页面）
app.mount("/static", StaticFiles(directory="static"), name="static")

# ============================================================================
# 配置参数
# ============================================================================

# MQTT Broker配置
MQTT_BROKER = os.getenv("MQTT_BROKER", "your_product_id.mqtts.acc.cmcconenet.cn")  # MQTT服务器地址
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))              # MQTT端口
MQTT_USERNAME = os.getenv("MQTT_USERNAME", "")             # MQTT用户名（可选）
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")             # MQTT密码（可选）

# OneNET平台配置
ONENET_PID = os.getenv("ONENET_PID", os.getenv("ONENET_PRODUCT_ID", "your_product_id"))        # 产品ID
ONENET_DEV = os.getenv("ONENET_DEV", os.getenv("ONENET_DEVICE_NAME", "stm32_env_monitor"))     # 设备名称
if "onenet" in MQTT_BROKER.lower() and not MQTT_USERNAME:
    MQTT_USERNAME = ONENET_PID

# DeepSeek AI配置
DEEPSEEK_API_KEY = os.getenv("DEEPSEEK_API_KEY", "")       # API密钥
DEEPSEEK_MODEL = os.getenv("DEEPSEEK_MODEL", "deepseek-chat")  # AI模型
DEEPSEEK_API_URL = os.getenv("DEEPSEEK_API_URL", "https://api.deepseek.com/v1/chat/completions")  # API地址

# MQTT主题定义（当前固件直连OneNET物模型）
TOPIC_DATA = os.getenv("MQTT_TOPIC_DATA", f"$sys/{ONENET_PID}/{ONENET_DEV}/thing/property/post")
TOPIC_CMD = os.getenv("MQTT_TOPIC_CMD", f"$sys/{ONENET_PID}/{ONENET_DEV}/thing/property/set")
TOPIC_RESP = os.getenv("MQTT_TOPIC_RESP", f"$sys/{ONENET_PID}/{ONENET_DEV}/thing/property/set_reply")
TOPIC_AI = os.getenv("MQTT_TOPIC_AI", f"$sys/{ONENET_PID}/{ONENET_DEV}/thing/property/post/reply")

# ============================================================================
# 全局状态变量
# ============================================================================

latest_sensor_data: dict = {}      # 最新传感器数据（JSON格式）
llm_analysis_result: str = ""      # 最新AI分析结果
response_queue: list = []          # 设备响应队列（用于指令下发后的响应等待）
mqtt_connected: bool = False       # MQTT连接状态标志
analysis_history: list = []        # AI分析历史记录
MAX_HISTORY = 10                  # 历史记录最大条数


def normalize_sensor_payload(payload: dict) -> dict:
    """把OneNET OneJSON或平铺JSON统一成平铺传感器数据。"""
    if not isinstance(payload, dict):
        return {}

    if "params" not in payload:
        return payload

    result = {}
    params = payload.get("params") or {}
    for key, value in params.items():
        if isinstance(value, dict) and "value" in value:
            result[key] = value["value"]
        else:
            result[key] = value
    return result


def build_property_set_payload(cmd: str) -> str:
    """把旧文本命令转换成固件可解析的OneNET property/set JSON。"""
    cmd = (cmd or "").strip().lower()
    mapping = {
        "relay_on": ("relay", 1),
        "relay_off": ("relay", 0),
        "buzzer_on": ("buzzer", 1),
        "buzzer_off": ("buzzer", 0),
        "led_on": ("led", 1),
        "led_off": ("led", 0),
        "rgb_red": ("rgb", 0),
        "rgb_green": ("rgb", 1),
        "rgb_blue": ("rgb", 2),
        "rgb_off": ("rgb", 3),
    }

    if cmd in mapping:
        key, value = mapping[cmd]
        return json.dumps({
            "id": str(int(time.time() * 1000)),
            "version": "1.0",
            "params": {key: {"value": value}}
        }, ensure_ascii=False)

    return cmd


def publish_device_command(cmd: str):
    mqtt_client.publish(TOPIC_CMD, build_property_set_payload(cmd))


def derive_smoke_detected(sensor_data: dict) -> bool:
    flame = bool(int(sensor_data.get("flame", 0) or 0))
    state = int(sensor_data.get("state", 0) or 0)
    temp = float(sensor_data.get("temp", 0) or 0)
    hum = float(sensor_data.get("hum", 0) or 0)
    temp_hum_warning = temp >= TEMP_WARN if "TEMP_WARN" in globals() else temp >= 35
    temp_hum_warning = temp_hum_warning or hum >= 80
    return (state == 2 and not flame) or (state == 1 and not flame and not temp_hum_warning)

# ============================================================================
# MQTT回调函数
# ============================================================================

def on_connect(client, userdata, flags, rc):
    """
    MQTT连接成功回调函数
    
    Args:
        client: MQTT客户端实例
        userdata: 用户数据（未使用）
        flags: 连接标志（未使用）
        rc: 连接返回码（0=成功，其他=失败）
    
    Returns:
        None
    """
    global mqtt_connected
    if rc == 0:
        mqtt_connected = True
        logger.info(f"✅ MQTT已连接至 {MQTT_BROKER}:{MQTT_PORT}")
        # 订阅传感器数据和设备响应主题
        client.subscribe(TOPIC_DATA)
        client.subscribe(TOPIC_RESP)
        logger.info(f"   订阅: {TOPIC_DATA}")
        logger.info(f"   订阅: {TOPIC_RESP}")
    else:
        mqtt_connected = False
        logger.error(f"❌ MQTT连接失败，返回码: {rc}")
        # 连接失败原因映射
        rc_messages = {
            1: "协议版本错误",
            2: "客户端ID被拒绝",
            3: "服务器不可用",
            4: "用户名或密码错误",
            5: "未授权"
        }
        logger.error(f"   原因: {rc_messages.get(rc, '未知错误')}")


def on_disconnect(client, userdata, rc):
    """
    MQTT断开连接回调函数
    
    Args:
        client: MQTT客户端实例
        userdata: 用户数据（未使用）
        rc: 断开原因码（0=正常断开，非0=异常断开）
    
    Returns:
        None
    """
    global mqtt_connected
    mqtt_connected = False
    logger.warning(f"⚠️ MQTT断开连接 (rc={rc})，将自动重连...")


def on_message(client, userdata, msg):
    """
    MQTT消息接收回调函数
    
    Args:
        client: MQTT客户端实例
        userdata: 用户数据（未使用）
        msg: 消息对象，包含topic和payload
    
    Returns:
        None
    """
    global latest_sensor_data, response_queue

    # 解码消息负载
    payload = msg.payload.decode('utf-8')
    logger.info(f"📩 收到消息 [{msg.topic}]: {payload[:200]}")

    # 根据主题处理不同类型的消息
    if msg.topic == TOPIC_DATA:
        # 传感器数据消息：解析JSON并触发AI分析
        try:
            data = json.loads(payload)
            latest_sensor_data = normalize_sensor_payload(data)
            # 在独立线程中调用DeepSeek进行AI分析，避免阻塞MQTT回调
            threading.Thread(target=analyze_with_deepseek, args=(latest_sensor_data,), daemon=True).start()
        except json.JSONDecodeError:
            logger.error(f"无效JSON数据: {payload}")

    elif msg.topic == TOPIC_RESP:
        # 设备响应消息：加入响应队列，用于指令下发后的响应等待
        response_queue.append({
            "timestamp": time.time(),
            "payload": payload
        })
        # 限制队列长度，最多保留100条
        if len(response_queue) > 100:
            response_queue.pop(0)


# ============================================================================
# MQTT客户端配置
# ============================================================================

# 创建MQTT客户端实例
mqtt_client = mqtt.Client(client_id=os.getenv("MQTT_CLIENT_ID", "stm32_ai_server"))
# 设置回调函数
mqtt_client.on_connect = on_connect
mqtt_client.on_disconnect = on_disconnect
mqtt_client.on_message = on_message

# 配置自动重连机制：最小延迟1秒，最大延迟30秒
mqtt_client.reconnect_delay_set(min_delay=1, max_delay=30)

# OneNET HTTP API推送配置（通过HTTP方式推送数据到OneNET平台）
ONENET_PID = os.getenv("ONENET_PID", ONENET_PID)        # 产品ID
ONENET_DEV = os.getenv("ONENET_DEV", ONENET_DEV)        # 设备名称
ONENET_ACCESS_KEY = os.getenv("ONENET_ACCESS_KEY", "")  # 访问密钥
# OneNET属性上报API地址
ONENET_API_URL = f"https://iot-api.heclouds.com/thing/property/post?topic=$sys/{ONENET_PID}/{ONENET_DEV}/thing/event/property/post"


def push_to_onenet(sensor_data: dict):
    """
    通过HTTP API推送传感器数据到OneNET平台
    
    Args:
        sensor_data: 传感器数据字典，包含temp、hum、flame、shock、state等字段（shock数据点上报烟雾标志）
    
    Returns:
        None
    """
    # 如果未配置OneNET参数，跳过推送
    if not ONENET_ACCESS_KEY or not ONENET_PID:
        return

    try:
        # 导入加密库（延迟导入以避免启动时依赖问题）
        import hmac, hashlib, base64
        
        # 生成鉴权Token
        # et: 过期时间（当前时间+3600秒）
        et = str(int(time.time()) + 3600)
        # res: 资源路径（产品级别）
        res = f"products/{ONENET_PID}"
        # 签名字符串格式：{et}\n{method}\n{res}\n{version}
        sign_str = et + '\nsha1\n' + res + '\n2018-10-31'
        # HMAC-SHA1签名
        sign = base64.b64encode(
            hmac.new(ONENET_ACCESS_KEY.encode(), sign_str.encode(), hashlib.sha1).digest()
        ).decode()
        # 组装Authorization头
        auth = f"version=2018-10-31&res={res}&et={et}&method=sha1&sign={sign}"

        # 构建OneNET OneJSON格式数据
        if 'params' in sensor_data:
            # 如果已经是OneJSON格式，直接透传并添加id字段
            body = sensor_data
            body['id'] = str(int(time.time() * 1000))
        else:
            # 否则包装成标准OneJSON格式
            params = {}
            for k in ['temp', 'hum', 'flame', 'shock', 'state']:
                if k in sensor_data:
                    params[k] = {'value': sensor_data[k]}
            body = {"id": str(int(time.time() * 1000)), "version": "1.0", "params": params}

        # 设置请求头
        headers = {"Content-Type": "application/json", "Authorization": auth}

        # 发送HTTP POST请求
        logger.info(f"OneNET: {json.dumps(body)[:120]}")
        resp = requests.post(ONENET_API_URL, headers=headers, json=body, timeout=10)
        
        # 处理响应
        if resp.status_code == 200:
            logger.info(f"📤 OneNET推送成功")
        else:
            logger.warning(f"OneNET推送失败: {resp.status_code} {resp.text[:200]}")
    except Exception as e:
        logger.error(f"OneNET推送异常: {e}")

# 如果配置了MQTT用户名，设置认证信息
if MQTT_USERNAME:
    mqtt_client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

# 初始化MQTT连接
if "onenet" in MQTT_BROKER.lower() and not MQTT_PASSWORD:
    logger.warning("未配置 MQTT_PASSWORD，跳过OneNET MQTT自动连接；REST API仍可启动")
else:
    try:
        # 连接MQTT Broker，保持连接时间60秒
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        # 启动MQTT客户端网络循环（后台线程）
        mqtt_client.loop_start()
        logger.info(f"正在连接MQTT服务器 {MQTT_BROKER}:{MQTT_PORT}...")
    except Exception as e:
        logger.error(f"❌ MQTT初始连接失败: {e}")
        logger.warning("将在STM32设备上报数据时自动重试连接")


def publish_ai_result(topic: str, result: dict):
    """
    发布AI分析结果到MQTT主题
    
    Args:
        topic: MQTT主题名称
        result: AI分析结果字典
    
    Returns:
        None
    """
    try:
        # 将结果序列化为JSON字符串
        payload = json.dumps(result, ensure_ascii=False)
        # 发布到MQTT主题
        mqtt_client.publish(topic, payload)
        logger.info(f"📤 发布AI结果至 {topic}")
    except Exception as e:
        logger.error(f"发布AI结果失败: {e}")


# ============================================================================
# Function Call工具定义 — AI可调用的硬件控制函数
# ============================================================================

# AI模型可调用的工具列表，用于实现自主决策硬件控制
# 每个工具定义包含名称、描述和参数规范，符合OpenAI Function Call格式
AI_TOOLS = [
    {
        "type": "function",
        "function": {
            "name": "control_relay",
            "description": "控制散热风扇/继电器开关。温度过高(≥35°C)、湿度异常、火焰或烟雾风险时自动开启风扇散热。",
            "parameters": {
                "type": "object",
                "properties": {
                    "action": {"type": "string", "enum": ["on", "off"], "description": "on=开风扇, off=关风扇"}
                },
                "required": ["action"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "control_buzzer",
            "description": "控制蜂鸣器报警。火焰、烟雾等危险情况时触发持续报警，预警状态时断续报警。",
            "parameters": {
                "type": "object",
                "properties": {
                    "action": {"type": "string", "enum": ["on", "off"], "description": "on=开蜂鸣器, off=关蜂鸣器"}
                },
                "required": ["action"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "control_rgb",
            "description": "控制RGB三色指示灯。正常=绿色, 预警=蓝色, 危险/火灾=红色闪烁, 关闭=灭灯。",
            "parameters": {
                "type": "object",
                "properties": {
                    "color": {"type": "string", "enum": ["red", "green", "blue", "off"], "description": "red=危险, green=正常, blue=预警, off=关闭"}
                },
                "required": ["color"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "control_led",
            "description": "控制板载LED指示灯。检测到异常时打开LED辅助指示。",
            "parameters": {
                "type": "object",
                "properties": {
                    "action": {"type": "string", "enum": ["on", "off"], "description": "on=开LED, off=关LED"}
                },
                "required": ["action"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "no_action",
            "description": "当前环境一切正常，无需执行任何硬件操作。",
            "parameters": {
                "type": "object",
                "properties": {},
                "required": []
            }
        }
    }
]

# 工具执行函数映射
def execute_tool_call(tool_name: str, arguments: dict) -> str:
    """
    执行AI调用的工具，通过MQTT下发指令到STM32设备
    
    Args:
        tool_name: 工具名称（如control_relay、control_buzzer）
        arguments: 工具参数字典（如{"action": "on"}）
    
    Returns:
        执行结果描述字符串
    """
    # 工具名称+参数到MQTT指令的映射表
    cmd_map = {
        ("control_relay", "on"): "relay_on",
        ("control_relay", "off"): "relay_off",
        ("control_buzzer", "on"): "buzzer_on",
        ("control_buzzer", "off"): "buzzer_off",
        ("control_rgb", "red"): "rgb_red",
        ("control_rgb", "green"): "rgb_green",
        ("control_rgb", "blue"): "rgb_blue",
        ("control_rgb", "off"): "rgb_off",
        ("control_led", "on"): "led_on",
        ("control_led", "off"): "led_off",
    }

    # 如果是no_action工具，无需执行任何操作
    if tool_name == "no_action":
        return "no_action"

    # 提取第一个参数值
    first_arg = list(arguments.values())[0] if arguments else ""
    # 在映射表中查找对应的MQTT指令
    cmd = cmd_map.get((tool_name, first_arg))
    
    if cmd:
        # 通过MQTT发布控制指令到设备
        publish_device_command(cmd)
        logger.info(f"🔧 AI执行工具: {tool_name}({first_arg}) → MQTT指令: {cmd}")
        return f"执行成功: {cmd}"
    
    # 未知工具或参数
    return f"未知工具: {tool_name}"


# ============================================================================
# AI智能分析 + Function Call自主决策
# ============================================================================

# AI调用限流控制
_last_ai_call_time = 0          # 上次AI调用时间戳
_last_executed_cmd = None       # 上次执行的命令（避免重复执行）
AI_MIN_INTERVAL = 10            # 最小调用间隔（秒），避免频繁调用API

def analyze_with_deepseek(sensor_data: dict):
    """
    调用DeepSeek大模型进行环境评估和Function Call自主决策
    
    Args:
        sensor_data: 传感器数据字典，包含temp、hum、flame、shock、state等字段（shock数据点上报烟雾标志）
    
    Returns:
        None
    """
    global llm_analysis_result, analysis_history, _last_ai_call_time, _last_executed_cmd

    # 如果未配置DeepSeek API Key，跳过AI分析
    if not DEEPSEEK_API_KEY:
        return

    # 限流控制：避免频繁调用AI API
    now = time.time()
    if now - _last_ai_call_time < AI_MIN_INTERVAL:
        return
    _last_ai_call_time = now

    # 提取传感器数据
    temp = sensor_data.get('temp', 0)      # 温度（°C）
    hum = sensor_data.get('hum', 0)        # 湿度（%）
    flame = sensor_data.get('flame', 0)    # 火焰状态（0=安全, 1=检测到火焰）
    smoke = derive_smoke_detected(sensor_data)  # 固件使用动态基准线判定，服务端以设备状态为准
    state = sensor_data.get('state', 0)    # 系统状态（0=正常, 1=预警, 2=危险）
    # 状态文本映射
    state_text = {0: '正常', 1: '预警', 2: '危险'}.get(state, '未知')

    # 构建系统提示，包含当前传感器数据和自主决策规则
    system_prompt = f"""你是智能环境监测AI助手，具备传感器分析和硬件控制能力。

当前传感器数据:
- 温度: {temp}°C (正常范围18-35°C)
- 湿度: {hum}% (正常范围30-80%)
- 火焰: {'⚠️检测到火焰!' if flame else '✅无火焰'}
- 烟雾: {'⚠️检测到烟雾!' if smoke else '✅无烟雾'}
- 系统状态: {state_text}

自主决策规则:
1. 火焰=有 或 烟雾=有 → 危险! 开蜂鸣器+红灯+风扇
2. 温度≥35°C 或 湿度≥80% → 预警! 开风扇+蓝灯
3. 温度>30°C → 建议开风扇
4. 一切正常但风扇开着 → 关风扇+绿灯
5. 温度<25°C 湿度正常 无异常 → 绿灯+关风扇+关蜂鸣器
6. 当前硬件状态未知，默认先执行安全操作

请先给出环境评估，再根据需要调用工具控制硬件。"""

    # 设置HTTP请求头
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {DEEPSEEK_API_KEY}"  # Bearer认证
    }

    # 构建API请求Payload
    payload = {
        "model": DEEPSEEK_MODEL,           # AI模型名称
        "messages": [
            {"role": "system", "content": system_prompt},  # 系统提示（包含传感器数据和决策规则）
            {"role": "user", "content": "分析当前环境数据，给出评估并决定是否需要执行硬件操作。"}  # 用户请求
        ],
        "tools": AI_TOOLS,                 # AI可调用的工具列表
        "tool_choice": "auto",             # 自动选择工具
        "max_tokens": 300,                 # 最大返回token数
        "temperature": 0.1                 # 温度参数（0.1表示确定性高）
    }

    try:
        logger.info("🤖 AI Agent 分析中...")
        start_time = time.time()  # 记录开始时间，用于计算耗时

        # 发送HTTP POST请求到DeepSeek API
        response = requests.post(DEEPSEEK_API_URL, headers=headers, json=payload, timeout=20)
        response.raise_for_status()  # 检查HTTP状态码
        result = response.json()     # 解析JSON响应

        # 计算AI分析耗时
        elapsed = time.time() - start_time
        
        # 提取响应消息
        message = result['choices'][0]['message']
        content = message.get('content', '')          # 文本分析结果
        tool_calls = message.get('tool_calls', [])    # Function Call列表

        # 处理文本分析结果
        if content:
            logger.info(f"📝 AI评估: {content.strip()[:100]}")
            llm_analysis_result = content.strip()[:120]  # 保存前120个字符

        # 处理Function Call（AI决定执行的硬件控制操作）
        executed_cmds = []  # 记录已执行的命令
        if tool_calls:
            logger.info(f"🔧 AI决定调用 {len(tool_calls)} 个工具")
            for tc in tool_calls:
                func = tc['function']
                tool_name = func['name']
                # 解析工具参数（可能是字符串或对象）
                try:
                    args = json.loads(func['arguments'])
                except json.JSONDecodeError:
                    args = {}
                # 执行工具调用
                result_text = execute_tool_call(tool_name, args)
                executed_cmds.append(f"{tool_name}({args}): {result_text}")
                logger.info(f"   → {result_text}")

            # 记录最后执行的命令，用于避免重复执行
            _last_executed_cmd = executed_cmds[-1] if executed_cmds else None

        # 构建综合分析结果（文本分析 + 执行命令）
        summary_parts = []
        if content:
            summary_parts.append(content.strip()[:80])  # 文本分析摘要（前80字符）
        if executed_cmds:
            # 提取命令名称（去掉参数和结果）
            cmd_summary = "; ".join([c.split(":")[0] for c in executed_cmds])
            summary_parts.append(f"[执行:{cmd_summary}]")

        # 组合最终结果
        llm_analysis_result = " | ".join(summary_parts) if summary_parts else "环境正常"

        logger.info(f"✅ AI决策完成 ({elapsed:.1f}s): {llm_analysis_result}")

        # 发布AI分析结果到MQTT主题（供STM32设备接收并显示）
        ai_payload = {
            "type": "ai_analysis",      # 消息类型标识
            "result": llm_analysis_result,  # AI分析结果文本
            "executed_tools": executed_cmds,  # 已执行的工具列表
            "timestamp": time.time()    # 时间戳
        }
        publish_ai_result(TOPIC_AI, ai_payload)

        # 添加到历史记录（用于API查询）
        analysis_history.append({
            "timestamp": time.time(),       # 时间戳
            "sensor_data": sensor_data,     # 原始传感器数据
            "result": llm_analysis_result,  # AI分析结果
            "tools_called": executed_cmds   # 调用的工具列表
        })
        # 限制历史记录最大条数
        if len(analysis_history) > MAX_HISTORY:
            analysis_history.pop(0)

    except requests.exceptions.Timeout:
        logger.error("DeepSeek API请求超时")
        llm_analysis_result = "AI分析超时，请稍后重试"
    except requests.exceptions.ConnectionError:
        logger.error("DeepSeek API连接失败")
        llm_analysis_result = "AI服务不可达"
    except requests.exceptions.HTTPError as e:
        logger.error(f"DeepSeek API HTTP错误: {e}")
        if "401" in str(e):
            llm_analysis_result = "AI API Key无效"
        elif "429" in str(e):
            llm_analysis_result = "AI请求频率过高"
        else:
            llm_analysis_result = f"AI服务错误: {e}"
    except Exception as e:
        logger.error(f"DeepSeek分析异常: {e}")
        llm_analysis_result = f"AI分析失败: {str(e)[:50]}"


# ============================================================================
# RESTful API
# ============================================================================

@app.get("/")
def root():
    """移动端控制面板"""
    from fastapi.responses import FileResponse
    return FileResponse("static/dashboard.html")


@app.get("/api")
def api_root():
    """API根路径"""
    return {
        "name": "STM32智能室内环境质量监测与安全预警系统",
        "version": "1.0.0",
        "mqtt_connected": mqtt_connected,
        "endpoints": [
            "/api/health",
            "/api/sensor-data",
            "/api/analysis",
            "/api/analysis-history",
            "/api/command?cmd=<command>",
            "/api/ai-control (POST)",
            "/docs (Swagger文档)"
        ]
    }


@app.get("/api/health")
def health_check():
    """系统健康检查"""
    return {
        "status": "healthy",
        "mqtt_connected": mqtt_connected,
        "mqtt_broker": MQTT_BROKER,
        "ai_configured": bool(DEEPSEEK_API_KEY),
        "sensor_count": len(latest_sensor_data),
        "timestamp": time.time()
    }


@app.get("/api/sensor-data")
def get_sensor_data():
    """获取最新传感器数据和AI分析"""
    return {
        "data": latest_sensor_data,
        "analysis": llm_analysis_result,
        "mqtt_connected": mqtt_connected,
        "timestamp": time.time()
    }


@app.get("/api/analysis")
def get_analysis():
    """获取AI分析结果"""
    return {
        "analysis": llm_analysis_result,
        "mqtt_connected": mqtt_connected,
        "timestamp": time.time()
    }


@app.get("/api/analysis-history")
def get_analysis_history():
    """获取AI分析历史记录"""
    return {
        "count": len(analysis_history),
        "history": analysis_history
    }


@app.get("/api/command")
def send_command(cmd: str):
    """
    发送控制指令到STM32设备

    支持的命令:
    - relay_on / relay_off: 继电器开关
    - buzzer_on / buzzer_off: 蜂鸣器开关
    - led_on / led_off: LED开关
    - rgb_red / rgb_green / rgb_blue / rgb_off: RGB灯控制
    - get_status: 获取设备状态
    - ai_analyze: 触发AI分析
    - set_threshold:<temp>,<hum>: 设置阈值
    """
    if not cmd:
        raise HTTPException(status_code=400, detail="请提供命令参数 ?cmd=<command>")

    if not mqtt_connected:
        raise HTTPException(status_code=503, detail="MQTT未连接，无法发送指令")

    cmd = cmd.strip().lower()
    logger.info(f"📤 发送指令: {cmd}")

    try:
        publish_device_command(cmd)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"指令发送失败: {e}")

    # 等待设备响应（最多5秒）
    timeout = 5
    start_time = time.time()
    while time.time() - start_time < timeout:
        if response_queue:
            resp = response_queue.pop(0)
            try:
                resp_data = json.loads(resp["payload"])
            except json.JSONDecodeError:
                resp_data = {"result": "ok", "raw": resp["payload"]}
            return {
                "command": cmd,
                "response": resp_data,
                "timestamp": resp["timestamp"]
            }
        time.sleep(0.1)

    return {
        "command": cmd,
        "response": {"result": "timeout", "msg": "设备无响应（可能是MQTT延迟）"},
        "timestamp": time.time()
    }


@app.post("/api/ai-control")
def ai_control(request: dict):
    """
    AI智能控制 - 用户用自然语言描述需求，AI自动决策并执行操作

    示例请求体:
    {
        "query": "太热了，帮我降温"
    }
    """
    query = request.get("query", "")
    if not query:
        raise HTTPException(status_code=400, detail="请提供 query 字段")

    if not DEEPSEEK_API_KEY:
        raise HTTPException(status_code=503, detail="未配置DeepSeek API Key")

    # 构建传感器摘要
    if latest_sensor_data:
        sensor_summary = f"""
当前环境状态：
- 温度: {latest_sensor_data.get('temp', 0)}°C
- 湿度: {latest_sensor_data.get('hum', 0)}%
- 火焰: {'有' if latest_sensor_data.get('flame', 0) else '无'}
- 烟雾: {'有' if derive_smoke_detected(latest_sensor_data) else '无'}
- 系统状态: {latest_sensor_data.get('state', 0)} (0=正常,1=预警,2=危险)
        """
    else:
        sensor_summary = "暂无传感器数据"

    prompt = f"""{sensor_summary}

用户请求: {query}

可用命令: relay_on, relay_off, buzzer_on, buzzer_off, led_on, led_off, rgb_red, rgb_green, rgb_blue, rgb_off, get_status

请根据用户请求和当前环境状态，决定是否需要执行硬件操作：
- 如果需要执行操作，只返回命令名（不要其他文字）
- 如果不需要操作，返回"none"
- 如果请求与可用命令无关，返回"unsupported"

只返回: none / unsupported / 或单个命令名"""

    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {DEEPSEEK_API_KEY}"
    }

    payload = {
        "model": DEEPSEEK_MODEL,
        "messages": [{"role": "user", "content": prompt}],
        "max_tokens": 20,
        "temperature": 0.1
    }

    logger.info(f"🤖 AI控制请求: {query}")

    try:
        response = requests.post(DEEPSEEK_API_URL, headers=headers, json=payload, timeout=10)
        response.raise_for_status()
        result = response.json()
        cmd = result['choices'][0]['message']['content'].strip().lower()

        # 清理响应（去除可能的引号和多余字符）
        cmd = cmd.replace('"', '').replace("'", '').replace('.', '').strip()

        logger.info(f"AI决策结果: {cmd}")

        if cmd == "none":
            return {
                "query": query,
                "command": None,
                "executed": False,
                "message": "AI判断无需执行硬件操作"
            }
        elif cmd == "unsupported":
            return {
                "query": query,
                "command": None,
                "executed": False,
                "message": "该请求超出设备控制范围"
            }
        else:
            # 执行命令
            if mqtt_connected:
                publish_device_command(cmd)
                return {
                    "query": query,
                    "command": cmd,
                    "executed": True,
                    "message": f"指令 '{cmd}' 已发送至设备"
                }
            else:
                return {
                    "query": query,
                    "command": cmd,
                    "executed": False,
                    "message": f"MQTT未连接，指令 '{cmd}' 未能发送"
                }

    except requests.exceptions.Timeout:
        raise HTTPException(status_code=504, detail="DeepSeek API请求超时")
    except requests.exceptions.HTTPError as e:
        logger.error(f"DeepSeek HTTP错误: {e}")
        raise HTTPException(status_code=502, detail=f"DeepSeek API错误: {e}")
    except Exception as e:
        logger.error(f"AI控制异常: {e}")
        raise HTTPException(status_code=500, detail=f"AI控制失败: {str(e)}")


@app.get("/api/device-state")
def get_device_state():
    """获取设备状态摘要"""
    if not latest_sensor_data:
        return {
            "online": False,
            "message": "暂无设备数据"
        }

    state_code = latest_sensor_data.get('state', 0)
    state_map = {0: "正常 🟢", 1: "预警 🔵", 2: "危险 🔴"}
    state_text = state_map.get(state_code, "未知")

    return {
        "online": True,
        "temperature": latest_sensor_data.get('temp', 0),
        "humidity": latest_sensor_data.get('hum', 0),
        "flame_detected": bool(latest_sensor_data.get('flame', 0)),
        "smoke_detected": derive_smoke_detected(latest_sensor_data),
        "state": state_text,
        "state_code": state_code,
        "last_analysis": llm_analysis_result,
        "mqtt_connected": mqtt_connected,
        "timestamp": time.time()
    }


# ============================================================================
# 启动
# ============================================================================

@app.get("/api/mqtt-test")
def mqtt_test():
    """
    MQTT通信诊断：用第二个客户端验证broker是否正常转发消息。
    如果这个测试返回 success，说明 broker 没问题，问题在 STM32 端。
    """
    result = {"broker": MQTT_BROKER, "tests": []}

    # Test 1: Check if main client is connected
    result["main_client_connected"] = mqtt_connected
    result["tests"].append({
        "name": "主客户端连接状态",
        "pass": mqtt_connected
    })

    # Test 2: Create a second client to test pub/sub on cmd topic
    test_topic = TOPIC_CMD
    test_msg = f"test_{int(time.time())}"
    received = []

    def on_test_message(client, userdata, msg):
        received.append(msg.payload.decode('utf-8'))

    try:
        test_client = mqtt.Client(client_id=f"stm32_test_{int(time.time())}")
        test_client.on_message = on_test_message
        test_client.connect(MQTT_BROKER, MQTT_PORT, 10)
        test_client.loop_start()
        time.sleep(0.5)

        # Subscribe to cmd topic
        test_client.subscribe(test_topic)
        time.sleep(0.5)

        # Publish test message from main client
        mqtt_client.publish(test_topic, build_property_set_payload(test_msg))

        # Wait for echo
        timeout = 3
        start = time.time()
        while time.time() - start < timeout:
            if received:
                break
            time.sleep(0.1)

        test_client.loop_stop()
        test_client.disconnect()

        if received:
            result["tests"].append({
                "name": f"消息转发测试 (topic={test_topic})",
                "pass": True,
                "message": f"发送'{test_msg}'，收到'{received[0]}'"
            })
        else:
            result["tests"].append({
                "name": f"消息转发测试 (topic={test_topic})",
                "pass": False,
                "message": f"发送'{test_msg}'，但3秒内未收到任何消息"
            })

    except Exception as e:
        result["tests"].append({
            "name": "测试客户端创建",
            "pass": False,
            "message": str(e)
        })

    result["all_pass"] = all(t["pass"] for t in result["tests"])
    return result


if __name__ == "__main__":
    import uvicorn

    host = os.getenv("SERVER_HOST", "0.0.0.0")
    port = int(os.getenv("SERVER_PORT", 8000))

    logger.info("=" * 60)
    logger.info("🏠 STM32智能室内环境质量监测与安全预警系统 - 服务器")
    logger.info("=" * 60)
    logger.info(f"MQTT服务器: {MQTT_BROKER}:{MQTT_PORT}")
    logger.info(f"AI模型: {DEEPSEEK_MODEL}")
    logger.info(f"API文档: http://{host}:{port}/docs")
    logger.info(f"健康检查: http://{host}:{port}/api/health")
    logger.info(f"MQTT诊断: http://{host}:{port}/api/mqtt-test")
    logger.info("=" * 60)

    uvicorn.run(app, host=host, port=port, log_level="info")
