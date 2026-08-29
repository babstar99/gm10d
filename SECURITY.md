# Security

## Supported version

Security fixes are applied to the latest development version and current release series.

## Deployment notes

`gm10d` is intended primarily for trusted LAN/home-lab environments.

### Metrics endpoint

The built-in HTTP endpoint has no authentication or TLS.

- Bind to `127.0.0.1` when only a local scraper is required.
- If binding to `0.0.0.0`, restrict port 9798 using host/network firewall policy to the scraper/network that needs it.
- Do not expose the metrics endpoint directly to the public Internet.

### MQTT

Use a dedicated MQTT identity with least-privilege ACLs. Keep the password outside the main configuration file and restrict it to the service group:

```bash
sudo chown root:gm10 /etc/gm10d.mqtt-password
sudo chmod 640 /etc/gm10d.mqtt-password
```

Version 0.1.3 does not expose TLS configuration options. Plain MQTT should therefore remain on a trusted network. If traffic must cross an untrusted network, protect it at the network layer or add/configure libmosquitto TLS support before deployment.

### Service account

The supplied systemd unit runs as a dedicated unprivileged `gm10` account with `dialout` as a supplementary group and enables several systemd hardening options.

## Reporting a security issue

Do not include credentials, private broker addresses, or other secrets in a public issue. Contact the repository maintainer privately using the mechanism published on the eventual GitHub repository.
