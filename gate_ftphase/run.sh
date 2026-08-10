ev() { # binario  net  -> una eval por linea
  { echo uci; echo "setoption name Threads value 1"; echo "setoption name EvalFile value $2/";
    echo isready
    while IFS= read -r f; do echo "position fen $f"; echo eval; done < gate_ftphase/fens.txt
    echo quit; } | "$1" 2>/dev/null | grep "^eval " | awk '{print $2}'
}
ev build_g0/src/talshand_exe /Users/miguel/Documents/GitHub/famE_nets/N256_H16_sk0__mix > gate_ftphase/e_ref.txt
ev build_g1/src/talshand_exe /private/tmp/wt_ftphase/gate_ftphase/nets/uniform  > gate_ftphase/e_uniform.txt
ev build_g1/src/talshand_exe /private/tmp/wt_ftphase/gate_ftphase/nets/bucket3_up   > gate_ftphase/e_b3up.txt
ev build_g1/src/talshand_exe /private/tmp/wt_ftphase/gate_ftphase/nets/bucket3_down > gate_ftphase/e_b3dn.txt
wc -l gate_ftphase/e_*.txt
