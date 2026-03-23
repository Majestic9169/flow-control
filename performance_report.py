import re
import os
import glob

def analyze_logs(directory="."):
    patterns = {
        'sent': re.compile(r"SENT_SEGMENT=(\d+)"),
        'drop': re.compile(r"PACKET_DROPPED"),
        'retransmit': re.compile(r"TIMEOUT_RETRANSMITTING=(\d+)")
    }

    header = f"{'Prob':<10} | {'Sent':<6} | {'Retrans':<8} | {'Drops':<6} | {'Avg Sends/Chunk':<15}"
    print(header)
    print("-" * len(header))

    log_files = sorted(glob.glob(os.path.join(directory, "*.log")))

    if not log_files:
        print("No .log files found in the directory.")
        return

    for file_path in log_files:
        filename = os.path.basename(file_path)
        
        prob_match = re.search(r"(\d+\.\d+)", filename)
        prob = prob_match.group(1) if prob_match else "N/A"

        stats = {'sent': 0, 'retrans': 0, 'drops': 0}
        unique_segments = set()

        with open(file_path, 'r') as f:
            for line in f:
                sent_match = patterns['sent'].search(line)
                if sent_match:
                    unique_segments.add(sent_match.group(1))
                
                if patterns['drop'].search(line):
                    stats['drops'] += 1

                retrans_match = patterns['retransmit'].search(line)
                if retrans_match:
                    stats['retrans'] += int(retrans_match.group(1))

        total_unique = len(unique_segments)
        total_attempts = total_unique + stats['retrans']
        avg_sends = total_attempts / total_unique if total_unique > 0 else 0

        print(f"{prob:<10} | {total_unique:<6} | {stats['retrans']:<8} | {stats['drops']:<6} | {avg_sends:<15.2f}")

if __name__ == "__main__":
    analyze_logs("./logs/")
