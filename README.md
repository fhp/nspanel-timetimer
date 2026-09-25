# NSPanel Time Timer

Een visuele aftel-timer voor de Sonoff NSPanel (EU) met de firmware van
[NSPanel-Easy](https://github.com/edwardtfn/NSPanel-Easy). Een gekleurde taartpunt krimpt tot de
eindtijd; daarnaast staan het label, de resterende tijd en de klok. Als de tijd om is piept het panel.

De timer tekent op de lege `canvas`-pagina van NSPanel-Easy; er is geen aangepaste TFT nodig. De voorganger
NSPanel_HA_Blueprint heeft die pagina niet en wordt niet ondersteund.

## Installatie

Voeg aan je device-YAML toe, ná het package van NSPanel-Easy:

```yaml
external_components:
  - source: github://fhp/nspanel-timetimer@main
    components: [timetimer]
    refresh: 300s

packages:
  # remote_package van NSPanel-Easy staat hierboven
  timetimer:
    url: https://github.com/fhp/nspanel-timetimer
    ref: main
    refresh: 300s
    files: [timetimer.yaml]
```

## Gebruik

| Actie | Parameters |
|---|---|
| `esphome.<device>_timer_start` | `eindtijd` (`"HH:MM"`), `label`, `kleur` (`rood`, `oranje`, `geel`, `groen`, `blauw`, `paars`; leeg = rood) |
| `esphome.<device>_timer_stop` | geen |

- De timer verschijnt zodra er 60 minuten of minder over zijn en houdt het scherm dan aan.
- Een tik laat het normale panel zien; na 30 seconden zonder aanraking komt de timer terug.
- Aan het eind piept het panel elke 5 seconden, maximaal 30 seconden; een tik stopt het piepen. Het alarmscherm blijft daarna nog
  een minuut staan.
- Een eindtijd die al voorbij is geldt voor morgen. Meer dan 12 uur vooruit wordt geweigerd.
- Het select `Timer thema` kiest tussen licht en donker.
- De alarmtoon is een RTTTL-string en is te vervangen via de substitutie `timetimer_tone` in je device-YAML.

Zie `examples/scripts.yaml` voor een script "timer over N minuten" en `examples/automations.yaml` voor een automatisering die het thema met de
zon laat meewisselen.

## Ontwikkelen

```bash
make test  # logica testen op de pc
uvx --with littlefs-python --with fatfs-ng esphome@2026.9.0 compile tests/nspanel-test.yaml  # firmware bouwen
```

De rekenlogica staat in `components/timetimer/timetimer.h` en heeft geen ESPHome-afhankelijkheden;
`timetimer.yaml` koppelt die aan het display, de buzzer en Home Assistant.
