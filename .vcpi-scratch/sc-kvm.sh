#!/bin/bash
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
cat > /tmp/claude-0/scrun/kvmrc <<'RC'
$_cpu_vm = "kvm"
$_cpu_vm_dpmi = "kvm"
$_ems = (8192)
$_jemm = (off)
$_vcpi = (on)
$_ext_mem = (6144)
RC
cd /tmp/claude-0/sc
echo "===== SCCD.EXE under KVM+VCPI ====="
timeout 300 $D -s -dumb -f /tmp/claude-0/scrun/kvmrc -o /tmp/kvm-sccd.log -D+Ek -K . -E "SCCD.EXE" </dev/null > /tmp/kvm-sccd.out 2>&1
echo "exit=$?"
tail -20 /tmp/kvm-sccd.out
echo "===== VCPI trace ====="
grep -n -i "VCPI\|monitor" /tmp/kvm-sccd.log | tail -40
