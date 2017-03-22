# Mailbox-Automation
This code utilizes a Node MCU (ESP8266), two LED strips and two magnet switches to automate mailbox lights and checks. It tracks the number of times mail has been deposited since the last time it was emptied. This design relies on the mailbox having separate openings for depositing and emptying mail.

MQTT is used to communicate with Home Assistant, in order to automate turning on and off lights depending on darkness outside as well as when hatch or door is opened.