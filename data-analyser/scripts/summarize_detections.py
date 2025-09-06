#!/usr/bin/env python3
"""
Seizure Detection Summary Script

This script processes all seizure detection logs and creates summary statistics.
It scans through all date folders and session files to count total detections.
"""

import os
import sys
import glob
from datetime import datetime
from collections import defaultdict

def count_lines_in_file(filepath):
    """Count non-comment lines in a detection log file."""
    try:
        with open(filepath, 'r') as f:
            lines = f.readlines()
        
        # Count non-comment lines (skip lines starting with #)
        count = 0
        for line in lines:
            line = line.strip()
            if line and not line.startswith('#'):
                count += 1
        
        return count
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return 0

def get_file_info(filepath):
    """Extract session info from filename and file content."""
    filename = os.path.basename(filepath)
    
    # Extract session start time from filename
    if filename.startswith('session_') and filename.endswith('.txt'):
        session_time = filename[8:-4].replace('_', ' ').replace('-', ':')
    else:
        session_time = "Unknown"
    
    # Try to get session start time from file header
    try:
        with open(filepath, 'r') as f:
            for line in f:
                if line.startswith('# Session started:'):
                    session_time = line.split(':', 1)[1].strip()
                    break
    except:
        pass
    
    return session_time

def scan_logs_directory(logs_dir="logs"):
    """Scan the logs directory and collect statistics."""
    if not os.path.exists(logs_dir):
        print(f"Logs directory '{logs_dir}' not found!")
        return
    
    print(f"Scanning logs directory: {logs_dir}")
    print("=" * 50)
    
    total_detections = 0
    date_stats = defaultdict(list)
    
    # Find all date directories
    date_dirs = [d for d in os.listdir(logs_dir) 
                 if os.path.isdir(os.path.join(logs_dir, d)) and d.count('-') == 2]
    
    if not date_dirs:
        print("No date directories found in logs folder.")
        return
    
    date_dirs.sort()
    
    for date_dir in date_dirs:
        date_path = os.path.join(logs_dir, date_dir)
        print(f"\n📅 Processing date: {date_dir}")
        
        # Find all session files in this date directory
        session_files = glob.glob(os.path.join(date_path, "session_*.txt"))
        
        if not session_files:
            print(f"  No session files found for {date_dir}")
            continue
        
        session_files.sort()
        date_total = 0
        
        for session_file in session_files:
            detections = count_lines_in_file(session_file)
            session_time = get_file_info(session_file)
            
            print(f"  📄 {os.path.basename(session_file)}: {detections} detections (started: {session_time})")
            
            date_total += detections
            date_stats[date_dir].append({
                'file': os.path.basename(session_file),
                'detections': detections,
                'session_time': session_time
            })
        
        print(f"  📊 Date total: {date_total} detections")
        total_detections += date_total
    
    # Create summary file
    create_summary_file(logs_dir, date_stats, total_detections)
    
    print("\n" + "=" * 50)
    print(f"TOTAL DETECTIONS ACROSS ALL DATES: {total_detections}")
    print(f"Summary saved to: {logs_dir}/info.txt")

def create_summary_file(logs_dir, date_stats, total_detections):
    """Create the summary info.txt file."""
    summary_path = os.path.join(logs_dir, "info.txt")
    
    try:
        with open(summary_path, 'w') as f:
            f.write("# Seizure Detection Summary\n")
            f.write(f"# Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"# Total detections: {total_detections}\n\n")
            
            f.write("## Summary by Date\n")
            f.write("=" * 30 + "\n\n")
            
            for date_dir in sorted(date_stats.keys()):
                date_total = sum(session['detections'] for session in date_stats[date_dir])
                f.write(f"### {date_dir}: {date_total} detections\n")
                
                for session in date_stats[date_dir]:
                    f.write(f"  - {session['file']}: {session['detections']} detections")
                    f.write(f" (started: {session['session_time']})\n")
                
                f.write("\n")
            
            f.write("## Overall Statistics\n")
            f.write("=" * 30 + "\n")
            f.write(f"Total detection files processed: {sum(len(sessions) for sessions in date_stats.values())}\n")
            f.write(f"Total dates with data: {len(date_stats)}\n")
            f.write(f"Total detections: {total_detections}\n")
            
            if total_detections > 0:
                avg_per_session = total_detections / sum(len(sessions) for sessions in date_stats.values())
                f.write(f"Average detections per session: {avg_per_session:.1f}\n")
        
        print(f"Summary file created: {summary_path}")
        
    except Exception as e:
        print(f"Error creating summary file: {e}")

def main():
    """Main function."""
    logs_dir = "logs"
    
    if len(sys.argv) > 1:
        logs_dir = sys.argv[1]
    
    print("🔍 Seizure Detection Summary Tool")
    print("=" * 40)
    
    scan_logs_directory(logs_dir)

if __name__ == "__main__":
    main()
