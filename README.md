can2joy
========

Use your car as a life-sized controller for your racing game on Linux!


Compatible cars
----------------

At the moment, only the following model has been tested:

  - Volkswagen Golf Mk 6


Compatible adapters
--------------------

Any CAN adapter supported by Linux' Socketcan stack should work.

It has been tested using:

  - 8devices USB2CAN


Compatible games
-----------------

Any game should work.

However my favorite demo tool is Need For Speed III, running in Wine.

Yes, a 1998 era Windows game, running on Wine on Linux, steered by an
actual car.


How to connect this to a car
-----------------------------

In order for can2joy to receive updates about the wheel, it needs to be
connected to a place in the car where this information flows freely.

BE AWARE THAT CONNECTING FOREIGN EQUIPMENT TO YOUR CAR IS ENTIRELY AT
YOUR OWN RISK. YOU'RE ON YOUR OWN IF YOUR CAR CRASHES, BURNS, EXPLODES,
OR DRIVES OVER YOUR CAT.

In a VW Golf Mk6, the OBD II connector is silent unless information is
requested. A relatively safe way of accessing this is the Infotainment
CAN bus, which connects to the radio/head unit, the Bluetooth module,
and the MDI/Media-In module. It's up to you to figure out how to connect
the car's CAN-High, CAN-Low, and GND to your CAN adapter.

On the software side, all we have to do there is to listen passively:

    ip link set can0 down
    ip link set can0 type can bitrate 100000
    ip link set can0 type can listen-only on
    ip link set can0 up

Then, you're ready to run can2joy.


How to run can2joy
-------------------

First, ensure that:
  - your Linux kernel supports uinput
  - the uinput module is loaded
  - /dev/uinput exists
  - your user has access to /dev/uinput

Then:

    make
    ./can2joy can0

...where can0 is to be replaced by the CAN interface you've
connected to your car.


License
--------

GNU General Public License 2

NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.
TINKERING WITH CARS IS AT YOUR OWN RISK.


Authors
--------

Copyright 2015-2018, Max Staudt
