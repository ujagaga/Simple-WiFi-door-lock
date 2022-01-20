# Simple-WiFi-door-lock

ESP8266 controlled electromagnetic lock. 
I keep forgetting to lock up my office, so I decided to install a door lock that gets locked up as soon as the door closes. This leaves me with a potential problem if I forget the keys inside the office.
For this reason I added an electromagnetic lock, so I can control it using my smartphone.
At first start, the software inside ESP8266 presents the user with a web page explaining how to set a lock key using a simple get request like:


	http://<device_ip_address>/?key=<Some HTML safe string up to 64 characters long>
	
	E.g. http://192.168.0.28/?key=SuperSecretUnlockKey123
	

This sets the desired access key, so you can then use that same url to unlock the door. Simplest is to bookmark this url and maybe create a shortcut on your smartphone desktop or application list.

There is a reset button onboard NodeMCU board, but you can use any ESP8266 board and add your own button. 
This button is used to reset the access key when held pressed longer than 5 seconds. The onboard LED blinks when the reset mode is activated.
If you hold it for longer than 15 seconds, the Over The Air update is triggered, so you can update the Arduino code wia WiFi. The LED will then blink faster.

## Security considerations

Using https protocol on ESP8266 is not simple nor practical, so it is best to stick to http. This does not offer high security, 
but if you only access the device in your local network (no access from internet), you should have no real danger of compromising your security 
as the actions and skill needed are probably way more than most poses. 
The reset button is the only way to change the access key, so anyone looking to gain access would need to physically access the device.
