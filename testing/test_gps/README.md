# Testing program

This is a testing program.



## Development

1. Set target at first time.

   ```
   idf.py set-target esp32s3
   ```



2. Build the project.

   ```
   idf.py build
   ```

      

3. If you using the dev-docker container, change the permission for the USB to UART device. For example the device is ttyACM0.
    ```
    sudo chmod 666 /dev/ttyACM0
    ```

    

4. Download to the target.

    ```
    idf.py flash -p /dev/ttyACM0
    ```

