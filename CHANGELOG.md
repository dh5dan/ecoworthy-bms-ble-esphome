# Änderungsprotokoll

## 2026-10-18

- BLE-Gerätename aus dem Advertising gelesen und als Textsensor `BMS BLE Gerätename` in der ESPHome-Web-GUI angezeigt.
- Der Name wird zusätzlich weiterhin im Log ausgegeben.
- Benötigte Web-GUI-MDI-Icons als lokale SVG-Daten in den www.js-Quellen eingebettet.
- Externes Nachladen des Bluetooth-Icons für den BLE-Client deaktiviert.

## 2026-10-17

- Anzeige `Letzter erfolgreicher Datensatz` in der ESPHome-Web-GUI ergänzt.
- Format: `TT.MM.JJJJ HH:MM:SS` in der konfigurierten Zeitzone `Europe/Berlin`.
- Aktualisierung erfolgt erst nach einem gültigen, vollständigen A1/A2-Datensatz.
- Bei noch nicht synchronisierter Uhr wird der Zeitstempel nicht aktualisiert und eine Warnung geloggt.
- Die Zeitstempel-Aktualisierung erzeugt keine laufende INFO-Logausgabe.
- Die Warnung bei noch nicht synchronisierter Uhr bleibt erhalten.
