# Wifi 5.2 Component

This components has been tested with: **ESP-IDF-Advanced-Template-Alpha-5.2**  
**This component includes both Wifi and SNTP functionality**

**Expected changes to arrive:**  
* Adding Secondary Host - Not in the software yet.
* Universal Wifi Provisioning (Functioning in the software BUT NOT DOCUMENTED YET)
___  
## Wifi Object  
One notable behavior that is important to observe is that the process of connecting to Wifi takes a fair amount of time. It is composed of 3 steps:
* **Connect to a Host** (finds the target modem/router and password negotiation is successful)
* **Obtaining an IP Address** (client DNCP calls for DHCP services in the host and obtains an IP address)
* **Contacting an SNTP server and requesting epoch time** (A timer server name is revolved to an Internet server address and time is retrieved)

Each of these steps are timed and if they run too long, a time-out occurs and the full connection is reset and restarted.

The full process under normal condition normally take 5 to 10 seconds.  If you try to terminate the process before a full connection is made, then the complexity of reclaiming resouces becomes more difficult to track.

The first and best way to disconnect is to wait until the connection process is complete first, then disconnect.  This is the lowest risk process.  It takes more time, but the results are more predictable.

The alternative choice is to invoke a **Shutdown** command.   This is a faster process but the paths for unexpected behavior are increased.

For example, if you are putting the unit to Sleep, the normal disconnection is the recommended approach.  But, if you are going to reboot or restart the object, then a Shutdown is permissable.

### Observational Note:
The Wifi connection/disconnection process has to be done just right for your unit to correctly enter power saving modes.  Through a very long series of tests and experimentation, it is clear that sloppy programming will lead to Esp modules that will run hotter than they should.   If you follow all the details, the results should be so good, that you feel very little extra heat on the Esp32 unit while it is sitting there idle wirelessly connected to a host. 

You may follow these links to Wifi documentation:
1) [Wifi Abstractions](./src/wifi/docs/wifi_abstractions.md)
2) [Wifi Block Diagram](./src/wifi/docs/wifi_blocks.md)
3) [Wifi Flowcharts](./src/wifi/docs/wifi_flowcharts.md)
4) [Wifi Operations](./src/wifi/docs/wifi_operations.md)
5) [Wifi Sequences](./src/wifi/docs/wifi_sequences.md)
6) [Wifi State Models](./src/wifi/docs/wifi_state_models.md)  
___  
## SNTP Object  
We create a wrapper object to abstract the IDF's Simple Network Time Protocol functions. This is a companion to the Wifi object and it does not run its own task.  The Wifi object calls the SNTP object's run function periodically to service all SNTP's needs.  One of the primary benefits having the Wifi task run the SNTP command loops is that we don't need any RTOS locking to share varibles between those two objects. 

You may follow these links to SNTP documentation:
1) [SNTP Abstractions](./src/sntp/docs/sntp_abstractions.md)
2) [SNTP Block Diagram](./src/sntp/docs/sntp_blocks.md)
3) [SNTP Flowcharts](./src/sntp/docs/sntp_flowcharts.md)
4) [SNTP Operations](./src/sntp/docs/sntp_operations.md)
5) [SNTP Sequences](./src/sntp/docs/sntp_sequences.md)
6) [SNTP State Models](./src/sntp/docs/sntp_state_models.md) 
___  

