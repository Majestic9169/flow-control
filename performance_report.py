import re
import os
import glob

def analyze_logs(directory="."):
    patterns = {
        'sent': re.compile(r"SENT_SEGMENT=(\d+)"),
        'drop': re.compile(r"PACKET_DROPPED"),
        'retransmit': re.compile(r"TIMEOUT_RETRANSMITTING=(\d+)")
    }

    header = f"{'Prob':<8} | {'Unique Sent':<12} | {'Retrans':<8} | {'Drops':<8} | {'Avg Sends/Chunk':<15}"
    print(header)
    print("-" * len(header))

    log_files = sorted(glob.glob(os.path.join(directory, "*.log")))

    for file_path in log_files:
        filename = os.path.basename(file_path)
        prob_match = re.search(r"(\d+\.\d+)", filename)
        prob = prob_match.group(1) if prob_match else "N/A"

        total_unique_segments = 0
        last_seen_seq = -1
        retrans_count = 0
        drop_count = 0

        with open(file_path, 'r') as f:
            for line in f:
                sent_match = patterns['sent'].search(line)
                if sent_match:
                    current_seq = int(sent_match.group(1))
                    
                    if current_seq > last_seen_seq:
                        total_unique_segments += (current_seq - last_seen_seq)
                    elif current_seq < last_seen_seq:
                        total_unique_segments += (256 - last_seen_seq) + current_seq
                    
                    last_seen_seq = current_seq
                
                if patterns['drop'].search(line):
                    drop_count += 1

                retrans_match = patterns['retransmit'].search(line)
                if retrans_match:
                    retrans_count += int(retrans_match.group(1))

        total_attempts = total_unique_segments + retrans_count
        avg_sends = total_attempts / total_unique_segments if total_unique_segments > 0 else 0

        print(f"{prob:<8} | {total_unique_segments:<12} | {retrans_count:<8} | {drop_count:<8} | {avg_sends:<15.2f}")

if __name__ == "__main__":
    analyze_logs("./logs/")
