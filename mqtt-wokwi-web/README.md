# MQTT + Wokwi Web Dashboard

This folder contains the web dashboard and Arduino firmware for the MQTT-oriented incubator approach.

## Run the dashboard

```powershell
npm install
npm start
```

The dashboard is served locally by `index.js`; the static UI is in `public/`.

## Hardware / simulation

- `Firmware.ino` is the Arduino sketch for the incubator controller and Wokwi-oriented simulation work.
- Configure the MQTT broker and topic values in the sketch before connecting a real device.
- Keep `node_modules/` local; it is intentionally excluded from Git.

## Project documents

The LaTeX source and exported reports are included for reference in this folder.

