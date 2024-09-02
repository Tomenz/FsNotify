# FsNotify

A linux systemd service to monitors directory's or files, an executes a user defined actions if a event is detected.

# Configuration

The configuration is is done in the fsnotify.cfg file.

```
[/home/user/Downloads]
itemflag=ONLYDIR

monitortyp=CREATE
filter=core.*
limit = 2 / 10s
actiontyp=system
actionpara=ls -l /home/user/Downloads > ./ls.out

monitortyp=DELETE
filter=core.*
actiontyp=system
actionpara=echo Deleted {NOTIFYITEM}/{NAME} >> ./ls2.out

[/home/user/test.sh]
monitortyp=OPEN
filter=.*
actiontyp=system
actionpara=echo OPEN {NOTIFYITEM} >> ./ls2.out

```

monitortyp = ACCESS | ATTRIB | CLOSE_WRITE | CLOSE_NOWRITE | CREATE |
             DELETE | DELETE_SELF | MODIFY | MOVE_SELF | MOVED_FROM |
             MOVED_TO | OPEN | CLOSE

filter     = Regular Expression to filter file or directory name

actiontyp  = system | syslog | addmonitor

actionpara = if actiontyp is "system" it can be any shell command or a shell
             script (*.sh)

             if actiontyp is "addmonitor" the actionpara is a string with a
             other monitor configuration.
             Example: actionpara = "itemflag=ONLYDIR\nmonitortyp=CLOSE_WRITE\nfilter=^[^\x23]+$\nactiontyp=system\nactionpara=../calcsha.sh {NOTIFYITEM}/{NAME}"
