# relaunchd

## Overview 

relaunchd is a service management daemon that is similar to the launchd(8) facility
found in the Darwin operating environment [1].

It was written from scratch based on the published API, and all code is available
under the ISC license. See the LICENSE file for more information.

It is currently under heavy development, and should not be used for anything important.
Be especially mindful that there is NO WARRANTY provided with this software.

## Status

The core functionality is working:
* loading JSON-formatted jobs with launchctl
* launching jobs
* socket-activated jobs via the Sockets key

There are some new features not found in the original launchd:
* JSON is used instead of XML
* you can launch programs that place themselves inside of a jail(8)
* a wrapper library that allows programs to use socket activation
  without modifying the source code.
   
Some things are not implemented yet:
* unloading jobs via launchctl
* cron functionality - StartInterval, StartCalendarInterval
* file and directory watches - WatchPaths, QueueDirectories
* restarting jobs if they crash - TimeOut, ExitTimeout, KeepAliveTimeout, ThrottleInterval
* resource limits - SoftResourceLimits, HardResourceLimits
* miscellaneous - LaunchOnlyOnce, inetdCompatibility, EnableGlobbing, RunAtLoad, Umask

Some things will probably never be implemented:
* oddities - LimitLoadToHosts, LimitLoadToHosts
* kernel and launchd debugging - Debug, WaitForDebugger
* Mach IPC
* the StartOnMount key - may require kernel support for filesystem mount notifications
* the original XML plist format; use JSON instead.
* hacks and workarounds - HopefullyExitsFirst, HopefullyExitsLast
* Darwin-specific things - EnableTransactions
* legacy keys - Disabled, OnDemand

## Socket activation

relaunchd uses a different mechanism for socket activation than the one that Darwin uses.

TODO -- document this

## Installation 

Currently, relaunchd has only been tested on FreeBSD 10, but should be portable to other BSD
operating systems. 

To install relaunchd, run the following commands:

	make
	sudo make install
	sudo /sbin/launchd

This will install the following executable commands:
	* launchd
	* launchctl

If you want to run jobs as your own user account, add the following line to your .xinitrc
file:

	/sbin/launchd

If you don't have an .xinitrc file, refer to the documentation for your desktop environment
to determine the best way to automatically execute a command when you login.


## Usage

At boot time, jobs will be automatic
	$HOME/launchd/agents
	/etc/launchd/agents
	/etc/launchd/daemons
        /usr/share/launchd/daemons


## Differences with Darwin launchd

There are some specific design choices that make relaunchd different from the or
iginal launchd found in Darwin.

The original launchd uses friendly MacOS-style names for the directories where it's
configuration files are stored.  For example, one of the directories is 
named /System/Library/LaunchDaemons. Relaunchd uses more traditional lowercase 
names, and stores it's files in:

	$HOME/launchd/agents
	/etc/launchd/agents
	/etc/launchd/daemons
        /usr/share/launchd/daemons
        /usr/share/launchd/agents

On FreeBSD, there is no compelling reason to have launchd run as pid #1, so 
relaunchd is designed to be started by the traditional init(8) command. Relaunchd currently 
coexists with the /etc/rc mechanism, until such time as all of the /etc/rc.d scripts can be converted to launchd jobs.

Instead of using the XML plist format, job manifests are specified using JSON.

In the Darwin implementation, there is a single launchd(8) process that is accessible to all user accounts.
In relaunchd, there is a "system launchd" process that runs as root and is not accessible by unprivileged users. 
Each unprivileged user may run their own launchd process under their own UID.

## References

[1] https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man8/launchd.8.html
