1. Fetch a binary SD card image from the GitHub releases page.

2. Burn the image to an SD card. Use a tool such as BalenaEtcher, Raspberry Pi Imager or Rufus for this purpose.

3. Insert the SD card into the slot on the DE10-Nano board. Power it on.

4. The board will obtain an IP address via DHCP. The MAC address of the Ethernet port is D6:7D:AE:B3:0E:BA. To change
network configuration (e.g. to modify the MAC address or to set up a static IP address), connect to the board through
the serial console. That is the USB mini port marked UART on the board. Use username root and password eit.

5. Connect to the board (via SSH or the serial console) and run `run_all_tests`. It should report `SUCCESS` when all
tests are complete. At the default 100 MHz streaming clock, the test run should take about 7 minutes.

Optionally (recommended):

6. Perform burn-in testing by running `run_all_tests_forever`. Let it run overnight or longer.
A short report and full log files are stored in `/var/volatile/pulsepins-test-logs`.

7. Change the root password from the default 'eit'.

8. Use `ssh-copy-id` to copy your public SSH key and enable passwordless logins.
