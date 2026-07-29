#!/bin/bash
NNUEU_NET=/tmp/lamano_famB/models/famB_psqt_bucketed/
export NNUEU_NET
exec /tmp/lamano_famB/build_split/src/talshand_exe "$@"
