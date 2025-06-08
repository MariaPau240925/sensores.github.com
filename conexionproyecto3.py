import json
import numpy as np
import paho.mqtt.client as mqtt
import requests
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

# Configuración InfluxDB
influx_url = "http://localhost:8086"
influx_token = "SU6MP8xKOcpe9ObnGpsHlSYvnxzEhfcQQzFjdOYYCkemA2Vg5h29eMHQKq1WWv45bTIOrIfIJVaQAFUPJsYiaQ=="
influx_org = "Communicaciones AD"
influx_bucket = "sensores"
influx_client = InfluxDBClient(url=influx_url, token=influx_token, org=influx_org)
write_api = influx_client.write_api(write_options=SYNCHRONOUS)

# Configuración MQTT 
mqtt_broker = "broker.hivemq.com"
mqtt_port = 1883
mqtt_topic = "grupo3/sensores"

# Configuracion Telegram 
telegram_token = "7643367023:AAFO0fu-udaLZxs6DpneCxbiEw536ZZOSEM"
telegram_chat_id = "1849231301"

# Función para enviar alerta a Telegram
def enviar_alerta_telegram(mensaje):
    url = f"https://api.telegram.org/bot{telegram_token}/sendMessage"
    payload = {"chat_id": telegram_chat_id, "text": mensaje}
    try:
        response = requests.post(url, data=payload)
        if response.status_code == 200:
            print("Alerta enviada a Telegram.")
        else:
            print("Error en Telegram:", response.text)
    except Exception as e:
        print("Excepción al enviar alerta:", e)

# Conexion MQTT para envio de mensajes
def on_connect(client, userdata, flags, rc):
    print("Conectado a MQTT con código:", rc)
    client.subscribe(mqtt_topic)

# Conexion MQTT para recibir mensajes
def on_message(client, userdata, msg):
    payload = msg.payload.decode().strip()
    print("Mensaje recibido:", payload)

    if not payload:
        print("Mensaje vacío, ignorado.")
        return

    try:
        data = json.loads(payload)
        ts = int(data.get("timestamp", 0))

        # Verificacion y procesamiento de sensor1 (acelerómetro)
        if "sensor1" in data:
            s1 = data["sensor1"]
            p1 = Point("acelerometro") \
                .field("Ax", s1["Ax"]) \
                .field("Ay", s1["Ay"]) \
                .field("Az", s1["Az"]) \
                .field("ruido", float(s1["ruido"])) \
                .field("snr", float(s1["snr"]))
            write_api.write(bucket=influx_bucket, org=influx_org, record=p1)
            print("📥 Acelerómetro enviado a InfluxDB.")

            if s1["snr"] == 1000:
                enviar_alerta_telegram("Alerta: SNR de s1 acelerómetro es 1000")

            # transformada rapida de Fourier (FFT) de Ax
            if not hasattr(on_message, "ax_buffer"):
                on_message.ax_buffer = []
            on_message.ax_buffer.append(s1["Ax"])
            if len(on_message.ax_buffer) > 64:
                on_message.ax_buffer.pop(0)
            if len(on_message.ax_buffer) == 64:
                ax_array = np.array(on_message.ax_buffer)
                fft_mags = np.abs(np.fft.fft(ax_array))[:32]
                fft_point = Point("fft_ax")
                for i, mag in enumerate(fft_mags):
                    fft_point.field(f"f{i}", float(mag))
                write_api.write(bucket=influx_bucket, org=influx_org, record=fft_point)
                print("FFT enviada a InfluxDB (fft_ax).")

        # Verificacion y procesamiento de sensor2 (temperatura)
        if "sensor2" in data:
            s2 = data["sensor2"]
            p2 = Point("temperatura") \
                .field("valor", s2["temperatura"]) \
                .field("ruido", float(s2["ruido"])) \
                .field("snr", float(s2["snr"]))
            write_api.write(bucket=influx_bucket, org=influx_org, record=p2)
            print("🌡 Temperatura enviada a InfluxDB.")

            if s2["snr"] == 1000:
                enviar_alerta_telegram("Alerta: SNR de s2 temperatura es 1000")

    except json.JSONDecodeError as e:
        print("Error de JSON:", e)
    except KeyError as e:
        print("Clave faltante en JSON:", e)
    except Exception as e:
        print("Error inesperado:", e)

# Conexion Cliente MQTT
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(mqtt_broker, mqtt_port, 60)
client.loop_forever()