# SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT
# boot.py
import esp32

NETWORK_TIMEOUT = 60

# Execute startup script, if not needed, delete the code below
if __name__ == "__main__":
    # Ensure display is in portrait mode (0) so the UIFlow2 startup menu is never chopped off
    try:
        import M5
        M5.begin()
        M5.Display.setRotation(0)
    except:
        pass

    from startup import startup
    from m5sync import sync
    import os

    nvs = esp32.NVS("uiflow")
    try:
        boot_option = nvs.get_u8("boot_option")
    except:
        boot_option = 1  # default

    startup(boot_option, NETWORK_TIMEOUT)
    if boot_option != 0:  # Run main.py directly
        sync.run()
    else:
        print("Skip sync")

    # copy OTA update file to main.py
    # main_ota_temp.py this file name is fixed
    try:
        s = open("/flash/main_ota_temp.py", "rb")
        f = open("/flash/main.py", "wb")
        f.write(s.read())
        s.close()
        f.close()
        os.remove("/flash/main_ota_temp.py")
    except:
        pass
