A comprehensive collection of tests covering most of the implemented functionality of PulsePins, along with some documentation for testing procedures.

* run_all_tests: runs (almost) all tests from the collection once
* run_all_tests_forever: repeatedly runs all tests for burn-in testing (logs are written under `/var/volatile/pulsepins-test-logs`; `report` contains the `pptest` version and bitstream timestamp headers plus successful-run timestamps; pass `-no-report-files` to skip the accumulating `report.run_N` files)
* command recipe files now live in `../recipes/`
* sequence fixture files such as `sequence.simple`, `sequence.simple.ppbin`, and `vcd.simple1` are copied together with the executable tests and are used by the `ppplay` playback checks
