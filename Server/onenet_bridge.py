"""
OneNET Bridge v5 — 双 paho-mqtt，加重连保护
"""
import os, sys, time, hmac, hashlib, base64, json, random
import paho.mqtt.client as mqtt

PID = os.getenv("ONENET_PID", os.getenv("ONENET_PRODUCT_ID", "your_product_id"))
DEV = os.getenv("ONENET_DEV", os.getenv("ONENET_DEVICE_NAME", "stm32_env_monitor"))
DEVICE_KEY = os.getenv("ONENET_DEVICE_KEY", "")
EMQX_TOPIC = os.getenv("EMQX_TOPIC", "")

def build_token():
    if not DEVICE_KEY:
        raise RuntimeError("请先设置 ONENET_DEVICE_KEY 环境变量")
    dk = base64.b64decode(DEVICE_KEY)
    et = str(int(time.time()) + 86400 * 365 * 10)
    res_u = f"products%2F{PID}%2Fdevices%2F{DEV}"
    res_p = f"products/{PID}/devices/{DEV}"
    sign = base64.b64encode(hmac.new(dk, f"{et}\nsha1\n{res_p}\n2018-10-31".encode(), hashlib.sha1).digest()).decode()
    sign = sign.replace('+','%2B').replace('/','%2F').replace('=','%3D')
    return f"version=2018-10-31&res={res_u}&et={et}&method=sha1&sign={sign}"

# ==== EMQX Client ====
def on_emqx_connect(c, u, f, rc):
    if rc == 0:
        print("[EMQX] Connected")
        if EMQX_TOPIC:
            c.subscribe(EMQX_TOPIC)
            print(f"[EMQX] Subscribed {EMQX_TOPIC}")

def on_emqx_message(c, u, msg):
    try:
        raw = json.loads(msg.payload.decode())
        onedata = {"id": str(int(time.time()*1000)), "version": "1.0", "params": {
            "temp": {"value": raw.get("temp",0)}, "hum": {"value": raw.get("hum",0)},
            "flame": {"value": raw.get("flame",0)}, "shock": {"value": raw.get("shock",0)},
            "state": {"value": raw.get("state",0)}}}
        onenet.publish(f"$sys/{PID}/{DEV}/thing/property/post", json.dumps(onedata))
        print(f"[Bridge] → OneNET: temp={raw.get('temp')}, hum={raw.get('hum')}, flame={raw.get('flame')}, shock={raw.get('shock')}")
    except Exception as e:
        print(f"[Bridge] Error: {e}")

emqx = None
if EMQX_TOPIC:
    emqx = mqtt.Client(client_id=f"bridge{random.randint(1000,9999)}")
    emqx.on_connect = on_emqx_connect
    emqx.on_message = on_emqx_message
    emqx.connect_async(os.getenv("EMQX_BROKER", "broker.emqx.io"), int(os.getenv("EMQX_PORT", "1883")), 60)
    emqx.loop_start()

# ==== OneNET Client ====
def on_onet_connect(c, u, f, rc):
    codes = {0:'OK',1:'proto',2:'id',3:'unavail',4:'auth',5:'unauth'}
    print(f"[OneNET] RC={rc} ({codes.get(rc,'?')})")

def on_onet_disconnect(c, u, rc):
    print(f"[OneNET] Disconnected (rc={rc}), will auto-reconnect...")

if not DEVICE_KEY:
    print("[OneNET] Missing ONENET_DEVICE_KEY, bridge stopped.")
    sys.exit(1)

onenet = mqtt.Client(client_id=DEV)
onenet.on_connect = on_onet_connect
onenet.on_disconnect = on_onet_disconnect
onenet.username_pw_set(PID, build_token())
onenet.reconnect_delay_set(min_delay=5, max_delay=30)

try:
    onenet.connect(os.getenv("ONENET_MQTT_BROKER", f"{PID}.mqtts.acc.cmcconenet.cn"), 1883, 60)
except Exception as e:
    print(f"[OneNET] Connect error: {e}")
onenet.loop_start()

print("=" * 50)
print("Bridge v5 running: EMQX ↔ OneNET")
print("=" * 50)

while True:
    time.sleep(5)
