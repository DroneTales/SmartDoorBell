# Умный дверной звонок для Apple Home

Этот репозиторий содерджит прошивку для устройства Apple HomeKit, о котором рассказывается в [этом](https://youtu.be/UexestUQARw) и [этом](https://youtu.be/UexestUQARw) видео.
 
**Используемые компоненты**

- Беспроводной дверной звонок 433Mhz
- ESP32C3FN4 Super Mini

**Используемые библиотеки Arduino**

- esp32 by Espressif Systems (board)
- HomeSpan
- PubSubClient
 
**Настройки Arduino IDE**

- Board: ESP32C3 Dev BModule
- ESP CDC On Boot: Enabled
- CPU Frequency: 80MHz (WiFi)
- Core Debug Level: None
- Erase All Flash Before Sketch Upload: Disabled
- Flash frequency: 80Mhz
- Flash Mode: QIO
- Flash Size: 4MB (32Mb)
- JTAG Adapter: Disabled
- Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)
- Upload Speed: 921600
- Zigbee Mode: Disabled
- Programmer: Esptool

## Конфигурация прокта

Проект может быть сконфигурирован как для работы с видеокамерами через HomeBridge, так и как простой дверной звонок.  

Если вы не используете видеокамеры в связке с HomeBridge, то найдите и закомментируйте строку  

`#define USE_HOMEBRIDGE_MQTT`  

Если же вы используйете видеокамеры, то в следующем разделе вы найдете информацию о том, как правильно сконфигурировать HomeBridge для работы с дверным звонком.  

По-умолчанию дверной звонок включает два переключателя: Звук и Уведомления. Первый управляет звуком дверного звонка, второй - уведомлениями Apple Home. Вы можете оставить только один переключатель. Для этого закомментируйте строку  

`#define ENABLE_NOTIFICATIONS_SWITCH`  

В этом случае останется только один выключатель, который будет полностью отключать и звук и уведомления.  

## Изпользование звонка совместно с видеокамерами

Этот раздел отписывает настройку HomeBridge для работы с дверным звонком с использованием камер. Если у вас нет камер или вы не используете HomeBridge, то этот раздел можно пропустить.  

### Установка MQTT брокера

Подключитесь к вашей Raspberry Pi (или другому устройству, где у вас установлен HomeBridge) по SSH и введите следующуе команды:  

`sudo apt-get update`  
`sudo apt-get upgrade`  
`sudo apt-get install mosquitto mosquitto-clients`  
`sudo systemctl enable mosquitto`  
`sudo nano /etc/mosquitto/mosquitto.conf`

Последняя команда откроет файл конфигурации MQTT брокера. Удалите все строки и вставьте следующие:  

```
per_listener_settings true

pid_file /run/mosquitto/mosquitto.pid

persistence true
persistence_location /var/lib/mosquitto/

log_dest file /var/log/mosquitto/mosquitto.log

include_dir /etc/mosquitto/conf.d

listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
```

Создайте нового пользователя для MQTT брокера. Для этого введите следующую команду:  

`sudo mosquitto_passwd -c /etc/mosquitto/passwd dronetales`

На запрос пароля введите `dronetales`.

Запустите MQTT сервер (брокер), введя команду:  

`sudo systemctl restart mosquitto`

### Настройка HomeBridge

Подключитесь к HomeBridge по Web интерфейсу. Выберите "Редактировать JSON". 

В разделе CameraUI замените настройки MQTT следующими:

```
"mqtt": {
    "active": true,
    "tls": false,
    "host": "127.0.0.1",
    "port": 1883,
    "username": "dronetales",
    "password": "dronetales"
},
```

Если в процессе создания MQTT пользователя вы использовали иные имя и пароль, то укажите их в параметрах "username" и "password".

Пролистайте вниз до раздела "cameras" и найдите там раздел "mqtt". Измените его как показано ниже. Если в вашем файле конфигурации такого раздела нет, то добавьте его сразу после раздела "videoanalysis".  

```
"mqtt": {
      "doorbellTopic": "doorcam/bell",
      "doorbellMessage": "RING",
},
```

Теперь добавьте следующую строку сразу перед секцией "videoConfig":

`"doorbell": true,`

Готово.

## Поддержать автора
 
Если вам интересно то, что я делаю, вы можете поддержать меня используя ссылки ниже:  

**BuyMeACoffee**: https://buymeacoffee.com/dronetales  
**Boosty**: https://boosty.to/drone_tales/donate  
 
**BTC**: bitcoin:1A1WM3CJzdyEB1P9SzTbkzx38duJD6kau  
**BCH**: bitcoincash:qre7s8cnkwx24xpzvvfmqzx6ex0ysmq5vuah42q6yz  
**ETH**: 0xf780b3B7DbE2FC74b5F156cBBE51F67eDeAd8F9a  



