# Firmware fro X2E Reference Sensor





## Directories

| Name           | Description                                             |
| -------------- | ------------------------------------------------------- |
| common         | Source files shared across.                             |
| example_device | A example LoRa device, sending a fix data periodically. |
|                |                                                         |



## Hints

Clone with submodules

```
git clone --recurse-submodules \
	git@gitlab.com:matchx/x2e_reference_sensor/x2e_ref-firmware.git
```



## Creating a new firmware

Copy from example device.

```
cp -R example_device new_device
```



Add LoRa component to the new firmware.

```
git submodule add \
	git@gitlab.com:matchx/x2e_reference_sensor/x2e_ref-lora_compon.git \
	new_device/components/lora_compon 
```

