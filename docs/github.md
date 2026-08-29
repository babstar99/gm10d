# Publishing to GitHub

Suggested repository name:

```text
gm10d
```

Suggested description:

```text
Minimal C daemon for the Black Cat Systems GM-10 radiation detector with MQTT/Home Assistant and Prometheus/VictoriaMetrics output.
```

Suggested topics:

```text
radiation
geiger-counter
gm10
linux
c
serial
rs232
ftdi
mqtt
home-assistant
prometheus
victoriametrics
```

## First push

After creating an empty GitHub repository (do not initialise it with another README/license), add the remote and push the existing history and tags:

```bash
git remote add origin git@github.com:<USERNAME>/gm10d.git
git push -u origin main
git push origin --tags
```

HTTPS is also fine:

```bash
git remote add origin https://github.com/<USERNAME>/gm10d.git
git push -u origin main
git push origin --tags
```

The repository already contains:

- reconstructed 0.1.0 → 0.1.3 Git history
- annotated version tags `v0.1.0`, `v0.1.1`, `v0.1.2`, `v0.1.3`
- GPL-2.0-or-later license and gm4lin attribution
- GitHub Actions build/test workflow
- systemd unit and example configuration
- Home Assistant/MQTT documentation
- VictoriaMetrics/vmagent documentation
- native UART and StarTech FTDI hardware validation notes
- contribution and security guidance

## First release

Create a GitHub release from tag `v0.1.3` and use the changelog entry for 0.1.3 as the release notes. GitHub can automatically generate source archives from the tag; a deterministic `git archive` release tarball can also be generated as described in `docs/releasing.md`.
