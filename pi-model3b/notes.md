# Config for the Raspberry pi model 3

Default user is admin/admin, sshd is setup

Startup in `/etc/rc.local` starts autoipd

On boot alogin is automatically logged in and this user starts adj
To prevent starting adj on boot, touch `/home/alogin/.hushboot`
Boot is defined in `/home/alogin/.bashrc`, so this user should not be used for other tasks.
Start script is called `./888` since this is possible to type on a number pad.


autoipd is used to reserve an IP address
`/var/tmp/autoip.dat` contains the chosen ip address `/var/log/ip-boot.log` lists the last 5 assigned IPs.

On boot the rpi broadcasts its IP address.
Use this to listen for the broadcasts

    netcat -blu 169.254.255.255 4000

Booting the rpi takes about 40 seconds, be patient

Wifi is disabled
sudo systemctl disable triggerhappy
rm /var/log/journal

