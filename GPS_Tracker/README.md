# Example LoRa Device

This is an example LoRa device. It will using the LoRa component and connect to the LoRaWAN. Then sending a fixed data every 120s.



## Development

1. Set target at first time.

   ```
   idf.py set-target esp32s3
   ```

    

2. Build the project.

   ```
   idf.py build
   ```
   
      

4. Change the permission for the USB to UART device. For example the device is ttyACM0.

   ```
   sudo chmod 666 /dev/ttyACM0
   ```

   

5. Download to the target.

   ```
   idf.py flash -p /dev/ttyACM0
   ```

