# SPI testing

This program test the SPI communication with SX1261, SX1280 and ST25R3911.



## Development

1. Set target at first time.

   ```
   idf.py set-target esp32s3
   ```



2. Build the project.

   ```
   idf.py build
   ```

      

3. If you using the dev-docker, change the permission for the USB to UART device. For example the device is ttyACM0.
    ```
    sudo chmod 666 /dev/ttyACM0
    ```

    

4. Download to the target.

    ```
    idf.py flash -p /dev/ttyACM0
    ```

