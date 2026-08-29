# MQTT and Home Assistant

## Topics

With the default identifiers:

```text
gm10/gm10-01/state
gm10/gm10-01/status

homeassistant/sensor/gm10-01_cpm/config
homeassistant/sensor/gm10-01_cps/config
homeassistant/sensor/gm10-01_pulses_total/config
```

The state payload is similar to:

```json
{
  "cpm": 15,
  "cps": 0.250000,
  "counts_10s": 2,
  "cpm_10s": 12,
  "pulses_total": 1942,
  "serial_connected": true
}
```

`status` is retained and contains `online` or `offline`. Discovery messages are retained. The state payload is not retained.

## Broker user and ACL

A minimal Mosquitto ACL for the `gm10` publishing identity is:

```text
user gm10
topic write gm10/gm10-01/#
topic write homeassistant/sensor/gm10-01_cpm/config
topic write homeassistant/sensor/gm10-01_cps/config
topic write homeassistant/sensor/gm10-01_pulses_total/config
```

Create/update the password with the broker's normal `mosquitto_passwd` workflow and store only the plaintext client password on the gm10d host, for example:

```text
/etc/gm10d.mqtt-password
```

Recommended permissions:

```bash
sudo chown root:gm10 /etc/gm10d.mqtt-password
sudo chmod 640 /etc/gm10d.mqtt-password
```

## Home Assistant discovery

Home Assistant should automatically create a device named:

```text
Black Cat Systems GM-10
```

with three entities:

```text
Radiation CPM
Radiation CPS
Radiation Pulses
```

If discovery messages are visible in MQTT but the device does not appear, verify that Home Assistant is connected to the same broker and that MQTT discovery is enabled with the expected discovery prefix.

## Bridged-broker installations

If `gm10d` publishes to one broker while Home Assistant listens to another, bridge both the state/availability topics and discovery topics.

On the broker that originates the bridge, the bridge stanza may contain:

```text
topic gm10/gm10-01/# out 0
topic homeassistant/sensor/gm10-01_cpm/config out 0
topic homeassistant/sensor/gm10-01_cps/config out 0
topic homeassistant/sensor/gm10-01_pulses_total/config out 0
```

The bridge's local identity must also be allowed to read those topics. For example:

```text
user bridge_homeassistant
topic read gm10/gm10-01/#
topic read homeassistant/sensor/gm10-01_cpm/config
topic read homeassistant/sensor/gm10-01_cps/config
topic read homeassistant/sensor/gm10-01_pulses_total/config
```

The remote bridge identity must have permission to publish the corresponding topics on the Home Assistant-side broker.

Because discovery messages are retained, they should be delivered when the bridge connects even if the discovery publish happened earlier.
