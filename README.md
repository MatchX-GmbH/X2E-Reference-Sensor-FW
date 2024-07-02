# Firmware fro X2E Reference Sensor





## Directories

| Name                 | Description                                                 |
| -------------------- | ----------------------------------------------------------- |
| common               | Source files shared across.                                 |
| example_device       | A example LoRa device, sending a fix data periodically.     |
| environmental_sensor | A Environmental Sensor sending X2E embedded sensor data.    |
| class_c_control      | A Class-C LoRa device, accepting command and control a pin. |



## Hints

Clone with submodules

```
git clone --recurse-submodules \
	https://github.com/MatchX-GmbH/X2E-Reference-Sensor-FW.git
```



## Creating a new firmware

Copy from example device.

```
cp -R example_device new_device
```



Add LoRa component to the new firmware.

```
git submodule add \
	https://github.com/MatchX-GmbH/X2E-Reference-Sensor-LoRa-lib.git \
	new_device/components/lora_compon 
```

