# Bash completion for the PulsePins `pptool` command family.

# This script is intended to be installed on the board as
# /etc/profile.d/pulsepins-completion.sh.

if [ -z "${BASH_VERSION:-}" ]; then
  return 0 2>/dev/null || exit 0
fi

case $- in
  *i*) ;;
  *) return 0 2>/dev/null || exit 0 ;;
esac

_pulsepins_shared_opts="
-quiet
-veryverbose
-verbosecheck
-int_clk
-ext_clk
-clk
-core_pll
-core_pll_charge_pump
-core_pll_bandwidth
-int_pll
-int_pll_charge_pump
-int_pll_bandwidth
-freq_rescale
-wait
-exit_delay
-ignore-errors
"

_pulsepins_ppfg_opts="
-freq -period -duty -servo -v0 -v1 -start0 -delay -p -m -i -trig -autotrig
-gate -gate_debug -burst -cont -t -n_max
"

_pulsepins_ppdelay_opts="
-p -m -delay -duration -v1 -v0 -t
"

_pulsepins_pptrig_opts="
-trig_int -trig_ext -trig_misc -trig_any -trig_all -trig_std
-invert_trig_result -invert_int -invert_ext -invert_misc
-mask_int -mask_ext -mask_misc -pio -debug
"

_pulsepins_ppqout_opts="
-out_sel1 -out_sel2 -out_sel3 -out_sel4 -out_and -out_or -out_xor -out_xnor -out_maj
-out_block8 -out_block16 -out_sum12 -out_sum1234 -out_diff12
-i1 -i2 -i3 -i4 -q1 -q2 -q3 -q4
-invert1 -invert2 -invert3 -invert4
-mask1 -mask2 -mask3 -mask4
-force1 -force2 -force3 -force4
-invert_out -mask_out -force_out
-report_pre -report_post -self_test -test
"

_pulsepins_ppvcd_opts="
-file -target -scale -force -check -read -timeout -t -dont_wait
"

_pulsepins_ppplay_opts="
-file -format -force -target -scale -check -read -timeout -t -dont_wait
"

_pulsepins_pptest_opts="
-c -v -v0 -v1 -t -i -iv -trig -p -m -r -n -check -timeout -dump-converted
-rnd -cycles -delay -repetitions -nr_replays -pre -mid -sep -post -report
-vmin -vmax -vstep -dwell -spi_clock -f
"

_pulsepins_ppcounter_opts="
-test1 -test2 -check
"

_pulsepins_ppread_opts="
-oe -timeout -rbmode -save-vcd -save-text -save-binary
"

_pulsepins_ppts_opts="
-nopps -sigA -selA -pps_in -pps_xtal -timeout -nr
"

_pulsepins_ppgpsdo_opts="
-kp -ki -clip -reject -dp -di -eps -avg -selA -pps_in -pps_xtal -timeout -nr -k -l -vmin -vmax
"

_pulsepins_ppfreq_opts="
-gate_time -gate_len -nr
"

_pulsepins_ppaux_opts="
-nr -wait -mode -file -ctr -ts
"

_pulsepins_pptemp_opts=""
_pulsepins_ppreset_opts=""
_pulsepins_pphelloworld_opts=""
_pulsepins_pptool_opts=""
_pulsepins_ppmstest_opts=""
_pulsepins_ppdmatest_opts=""

_pulsepins_all_commands="
pptool
pptest
ppmstest
ppdmatest
ppfg
ppreset
pptrig
ppdelay
ppqout
ppaux
ppcounter
ppts
ppgpsdo
pptemp
ppfreq
ppread
ppplay
ppvcd
pphelloworld
"

_pulsepins_cmd_opts() {
  case "$1" in
    ppfg) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppfg_opts")" ;;
    ppdelay) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppdelay_opts")" ;;
    pptrig) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_pptrig_opts")" ;;
    ppqout) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppqout_opts")" ;;
    ppplay) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppplay_opts")" ;;
    ppvcd) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppvcd_opts")" ;;
    pptest) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_pptest_opts")" ;;
    ppcounter) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppcounter_opts")" ;;
    ppread) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppread_opts")" ;;
    ppts) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppts_opts")" ;;
    ppgpsdo) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppgpsdo_opts")" ;;
    ppfreq) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppfreq_opts")" ;;
    ppaux) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppaux_opts")" ;;
    pptemp) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_pptemp_opts")" ;;
    ppreset) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppreset_opts")" ;;
    pphelloworld) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_pphelloworld_opts")" ;;
    ppmstest) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppmstest_opts")" ;;
    ppdmatest) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_ppdmatest_opts")" ;;
    pptool) printf '%s\n' "$(_pulsepins_join_opts "$_pulsepins_pptool_opts")" ;;
    *) printf '%s\n' "" ;;
  esac
}

_pulsepins_join_opts() {
  printf '%s %s' "$_pulsepins_shared_opts" "$1"
}

_pulsepins_complete_files() {
  local cur=$1
  compopt -o filenames 2>/dev/null
  COMPREPLY=( $(compgen -f -- "$cur") )
}

_pulsepins_complete() {
  local cur prev cmd opts
  COMPREPLY=()
  cur=${COMP_WORDS[COMP_CWORD]}
  prev=${COMP_WORDS[COMP_CWORD-1]}
  cmd=${COMP_WORDS[0]}

  case "$prev" in
    -file)
      _pulsepins_complete_files "$cur"
      return 0
      ;;
    -format)
      COMPREPLY=( $(compgen -W "vcd text binary" -- "$cur") )
      return 0
      ;;
  esac

  opts=$(_pulsepins_cmd_opts "$cmd")
  COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
  return 0
}

complete -F _pulsepins_complete pptool
complete -F _pulsepins_complete pptest ppmstest ppdmatest ppfg ppreset pptrig ppdelay ppqout
complete -F _pulsepins_complete ppaux ppcounter ppts ppgpsdo pptemp ppfreq ppread ppplay ppvcd pphelloworld
