#!/bin/bash
# Usage: ./test_p_vals.sh <testfile>
# Run from project root.

if [ $# -ne 1 ]; then
  echo "usage: $0 <testfile>"
  exit 1
fi

TESTFILE="$1"
KSOCKET_H="lib/ksocket.h"
DAEMON_LOG="/tmp/ktp_daemon.log"
RECV_FILE="/tmp/ktp_recv.bin"
FILESIZE=$(wc -c < "$TESTFILE" | tr -d '[:space:]')
TOTAL_MSGS=$(( (FILESIZE + 511) / 512 ))

make -C lib -s
make -C sys -s
make -C app -s

echo "----------------------------------------------"
echo "  p     total_sends  msgs  avg_sends_per_msg"
echo "----------------------------------------------"

for p in 0.05 0.10 0.15 0.20 0.25 0.30 0.35 0.40 0.45 0.50; do

  sed -i "s/#define DROP_PROB .*/#define DROP_PROB ${p}f/" "$KSOCKET_H"
  make -C lib -s
  make -C sys -s
  make -C app -s

  rm -f /dev/shm/ktp_socket_table

  sys/ksocketd > "$DAEMON_LOG" 2>&1 &
  DAEMON_PID=$!

  for i in $(seq 1 30); do
    [ -e /dev/shm/ktp_socket_table ] && break
    sleep 0.1
  done

  if [ ! -e /dev/shm/ktp_socket_table ]; then
    echo "  $p  ERROR: daemon failed to start"
    kill $DAEMON_PID 2>/dev/null
    continue
  fi

  app/user2 9001 127.0.0.1 9000 "$RECV_FILE" > /dev/null 2>&1 &
  USER2_PID=$!
  sleep 0.2

  app/user1 9000 127.0.0.1 9001 "$TESTFILE" > /dev/null 2>&1
  sleep 2

  kill $USER2_PID  2>/dev/null
  kill $DAEMON_PID 2>/dev/null
  wait $USER2_PID  2>/dev/null
  wait $DAEMON_PID 2>/dev/null
  sleep 0.5

  # match any socket number — user1 may get slot 0 or 1 depending on order
  NEW=$(grep -c ": sent seq=" "$DAEMON_LOG" 2>/dev/null | tr -d '[:space:]')
  NEW=$(( ${NEW:-0} + 0 ))

  RETRANS=$(grep ": timeout, retransmitting" "$DAEMON_LOG" 2>/dev/null \
            | awk '{print $NF}' | awk '{s+=$1} END {print s+0}' | tr -d '[:space:]')
  RETRANS=$(( ${RETRANS:-0} + 0 ))

  TOTAL=$(( NEW + RETRANS ))
  AVG=$(echo "scale=2; $TOTAL / $TOTAL_MSGS" | bc)

  printf "  %-6s  %-11d  %-5d  %s\n" "$p" "$TOTAL" "$TOTAL_MSGS" "$AVG"

done

echo "----------------------------------------------"