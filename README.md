# batnot-daemon

sends battery notifications when reaches certain user provided level.  It works as daemon.

I do not plan on making it work on any other machine than mine.

---

By default root directory is set to `$HOME`.

It requires `.batnot_discharging_warning` files with space separated numbers. If battery level drops to any of those numbers or below it will send a notification. Smallest percentage in the file will be notified with **critical** urgency. 

## example:
```.txt
10 15 20
```
Here notification will be sent when battery level drops to 20, 15 and 10 in percents. When it drops to 10% notification will have critical level of urgency.

If you are sure the process is not already running and can't execute it remove `.batnot.pid` file.

All logs are in `.batnot.log` file.

Default update time is set to 30 seconds.

# How to stop batnot:
Send signal **SIGTERM** to the process, PID (Process ID) can be found in `.batnot.pid` file.

# Compilation
```
gcc -O3 -Wall batnot.c -o batnot-daemon
```
(of course different flags and output file name can be provided)
