# Clock domains and clock-domain-crossing conventions

Table of clock domains for each signal (not exhaustive, only time critical signals are listed here)

| Name              | Domain  | Assignment                      | Comments    |
| -------           | ------- | -----------                     | ----------- |
| gate              | -       | pin                             | |
| rdreq             | st      | continuously in streamer.sv     | |
| trigger_activated | st      | continuously in streamer.sv     | |
| trigger_o         | st      | sync in and_trigger.sv          | |

Convention:
- empty suffix: external signal / pin
- _co suffix: synchronized in core clock domain
- _st suffix: synchronized in streamer clock domain
