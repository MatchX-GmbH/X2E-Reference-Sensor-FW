# Example LoRa Device

This is an example LoRa device. It using the LoRa Component to connect to the LoRaWAN. Then sending data every 120s.



## Development

1. Set target at first time.

   ```
   idf.py set-target esp32s3
   ```

    

2. Build the project.

   ```
   idf.py build
   ```
   
      

4. Download to the target.

   ```
   idf.py flash -p /dev/ttyACM0
   ```




# Main software structure

![SoftwareStructure](doc/SoftwareStructure.png)

# Flow for AppOp

```mermaid
flowchart TD
	Start-->IsJoined[Is Joined?]
	IsJoined-- No -->JoinSleep["Light Sleep (Join interval)"]
	JoinSleep-->IsJoined
	IsJoined-- Yes -->SampleData[Sample data for 5s]
	SampleData-->SendData[Send data]
	SendData-->IsSendDone[Done?]
	IsSendDone-- No -->SendDataSleep["Light Sleep (retry interval)"]
	SendDataSleep-->IsSendDone
	IsSendDone-- Yes -->CheckRx[Check Rx]
	CheckRx-->CheckLink[Is link broken?]
	CheckLink-- Yes --> IsJoined
	CheckLink-- No -->IntervalSleep["Light Sleep (data interval)"]
	IntervalSleep-->SampleData
	
```

