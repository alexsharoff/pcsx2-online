PCSX2 Online
============

PCSX2 Online is a fork of open-source PlayStation 2 emulator PCSX2 with peer-to-peer netplay mode. Now you can play PS2 games with your friends on the internet as if you were in the same room!

Grab the latest version here http://www.mediafire.com/?r3c1ejv5m85ekrv

How to start netplay
--------------------

System -> Boot Netplay. The connection dialog is pretty self-explanatory.

What you should know before playing
-----------------------------------

* To avoid desyncs do not change any settings except for the controller plugin configuration. It's not like it will definitely desync if you change anything, but the possibility escalates.

* During netplay, emulation settings are temporarily reset to defaults with all speedhacks disabled, so performance may drop a bit. And by 'emulation settings' I mean Config->Emulation Settings dialog, so plugin settings remain intact.

Features
--------

* Peer-to-peer network connection, no centralized servers needed
* Automatic input delay detection

(Official) Working games list
-------------------------------

* Melty Blood Actress Again
* Tekken 5
* Capcom vs SNK 2
* Guilty Gear XX Accent Core
* Hokuto no Ken
* King of Fighters 2002 Unlimited Match
* Naruto: Ultimate Ninja 3

Working with frequent desyncs
-----------------------------

* Fate/Unlimited Codes
* King of Fighters XI

Changelog
---------
<pre>2012.01.22
This build doesn't force Skip Mpeg hack to be enabled, as it's incompatible with some games.

2011.12.02
1. Some cases of emulator hanging during netplay are fixed;
2. Compatibility is improved (for example, Tekken 5 now works);
3. Crashes during netplay should be less frequent now.

2011.11.12
Desyncs should now occur much less frequently than in previous version.

2011.10.31
1. Booting from physical disc/mounted ISO works;
2. Connectivity problems fixed.
3. Netplay or replay playback can be started only after clean emulator start (as it always desyncs otherwise).

2011.10.27
1. Improved GUI;
2. Replays (Press F4 to toggle fast-forward when watching);
3. Input delay fine-tuning (Available to hosting side);
4. Option to make memory card read-only (Available to hosting side, client's MCD is always read-only);
5. Bugfixes.

2011.10.09:
1. Client side downloads memory card from hosting side to stay in sync. When the session ends, saved state persist only on hosting side;
2. Frameskipping after slowdown due to packet loss should be fixed now;
3. Some stability issues addressed.
</pre>

Roadmap
-------
* Desync detection/correction;
* Support more games.
