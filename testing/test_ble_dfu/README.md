# Testing program

This is a testing program.



## Development

1. Build the project.

   ```
   idf.py build
   ```

      

2. If you using the dev-docker container, change the permission for the USB to UART device. For example the device is ttyACM0.
    ```
    sudo chmod 666 /dev/ttyACM0
    ```

    

3. Download to the target and bring up debug console.

    ```
    idf.py flash -p /dev/ttyACM0 monitor
    ```



4. Generate the zip file for Nodric DFU mobile APP.

   ```
   python3 gen_dfu_package.py build/test_ble_dfu.bin
   ```

   

   
