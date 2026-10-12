#!/bin/sh
# TCP-related kernel settings as seen from a fresh network namespace (defaults of this kernel).
sysctl net.ipv4.tcp_congestion_control net.ipv4.tcp_available_congestion_control net.ipv4.tcp_rmem net.ipv4.tcp_wmem net.core.rmem_max net.ipv4.tcp_sack net.ipv4.tcp_timestamps net.ipv4.tcp_window_scaling net.ipv4.tcp_syncookies net.ipv4.tcp_fin_timeout net.ipv4.tcp_retries2 net.ipv4.tcp_syn_retries net.ipv4.tcp_ecn net.ipv4.tcp_slow_start_after_idle
