#!/bin/bash
exec > /tmp/scx.log 2>&1
export DOSEMU2_COMCOM_DIR=/usr/local/share/comcom32
D=/home/user/dosemu2/bin/dosemu
mkdir -p /tmp/claude-0/scrun
cat > /tmp/claude-0/scrun/kvmrc <<'RC'
$_cpu_vm = "kvm"
$_cpu_vm_dpmi = "kvm"
$_ems = (8192)
$_jemm = (off)
$_vcpi = (on)
$_ext_mem = (6144)
$_ems_frame = (0xe000)
$_ems_conv_pages = (0)
RC
cd /tmp/claude-0/sc
timeout 600 $D -dumb -s -f /tmp/claude-0/scrun/kvmrc -o /tmp/kvm-sccd.log -D+E -K . -E "SCCD.EXE" </dev/null > /tmp/kvm-sccd.out 2>&1
echo "dosemu exit=$?"
