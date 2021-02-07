# Bluetooth details
Les radios OpenAVRc sont équipées d'un module Bluetooth de type HC-05.
Elles peuvent être configurées soit en 'maître' soit en 'esclave'.
Seule une radio définie en 'maître' peut lancer un scan afin de trouver une autre radio autour d'elle qui elle devra être définie en 'esclave'.

L'option Bluetooh permet de connecter deux radios en mode écolage.
Mais il est maintenant aussi possible d'utiliser un simulateur de vol (Realflight ou autre) ou de conduite (Vrc Pro) via ce module Bluetooth.
Deux options sont possible:
1. Le module Bluetooth côté réception, associé à un Arduino Pro Micro, simule un signal CPPM.
2. Le module Bluetooth côté réception, associé à un Arduino Pro Micro, simule un Joystick.
 
 Le code Arduino est téléchargeable [ici](https://github.com/Ingwie/OpenAVRc_Dev/tree/V3/PCB/Bluetooth/OpenAVRcBT_ToSimulator).

## Utiliser le module réception en mode PPM
 Configurer dans le code OpenAVRcBT_ToSimulator.ino, ligne 79,  **#define MODE PPM**

## Utiliser le module réception en mode Joystick
 Configurer dans le code OpenAVRcBT_ToSimulator.ino, ligne 79,  **#define MODE JOYSTICK**


