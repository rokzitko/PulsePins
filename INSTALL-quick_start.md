1. Fetch a binary SD card image from the GitHub releases page.

2. Burn the image to an SD card. Use a tool such as BalenaEtcher, Raspberry Pi Imager or Rufus for this purpose.

3. Insert the SD card in the slot of the DE10 Nano card. Power on.

4. The board will obtain the IP address via DHCP. The MAC address of the Ethernet port is D6:7D:AE:B3:0E:BA. To change
network configuration (e.g. to modify MAC address or to setup static IP address), one can connect to the board through
the serial console. That is the USB mini port marked UART on the board. Use username root and password eit.

5. Connect to the board (via ssh or on serial console) and run "run_all_tests". It should report "PASSED" when all
tests are done. At the default 100MHz streaming clock, the testing should take about 7 minutes.

Optionally (recommended):

6. Perform burn-in testing by running "run_all_tests_forever". Let it run overnight or longer.
A short report is stored in text file "results", while full log files are stored in /var/volatile.

7. Change the root password from the default 'eit'.

8. Use ssh-copy-id to copy your public ssh key and enable passwordless logins.
