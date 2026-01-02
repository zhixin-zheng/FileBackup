import os
import stat
import pwd
import grp
import sys
from datetime import datetime

def inspect_file(path):
    if not os.path.exists(path) and not os.path.islink(path):
        print(f"Error: Path '{path}' does not exist.")
        return

    try:
        # Use lstat to not follow symlinks
        st = os.lstat(path)
    except OSError as e:
        print(f"Error accessing '{path}': {e}")
        return

    print(f"File Attributes for: {path}")
    print("-" * 40)

    # 1. File Type
    if stat.S_ISLNK(st.st_mode):
        file_type = "Symbolic Link"
        target = os.readlink(path)
    elif stat.S_ISDIR(st.st_mode):
        file_type = "Directory"
    elif stat.S_ISREG(st.st_mode):
        file_type = "Regular File"
    elif stat.S_ISFIFO(st.st_mode):
        file_type = "FIFO (Named Pipe)"
    elif stat.S_ISSOCK(st.st_mode):
        file_type = "Socket"
    elif stat.S_ISCHR(st.st_mode):
        file_type = "Character Device"
    elif stat.S_ISBLK(st.st_mode):
        file_type = "Block Device"
    else:
        file_type = "Unknown"

    print(f"Type:           {file_type}")
    
    if stat.S_ISLNK(st.st_mode):
        print(f"Link Target:    {target}")

    # 2. Permissions
    print(f"Permissions:    {oct(stat.S_IMODE(st.st_mode))} ({stat.filemode(st.st_mode)})")

    # 3. Ownership
    try:
        user_name = pwd.getpwuid(st.st_uid).pw_name
    except KeyError:
        user_name = "Unknown"
    
    try:
        group_name = grp.getgrgid(st.st_gid).gr_name
    except KeyError:
        group_name = "Unknown"

    print(f"Owner (UID):    {user_name} ({st.st_uid})")
    print(f"Group (GID):    {group_name} ({st.st_gid})")

    # 4. Size
    print(f"Size:           {st.st_size} bytes")

    # 5. Time
    mtime = datetime.fromtimestamp(st.st_mtime).strftime('%Y-%m-%d %H:%M:%S')
    print(f"Last Modified:  {mtime}")

    # 6. Device Numbers (for device files)
    if stat.S_ISCHR(st.st_mode) or stat.S_ISBLK(st.st_mode):
        print(f"Device Major:   {os.major(st.st_rdev)}")
        print(f"Device Minor:   {os.minor(st.st_rdev)}")

    # 7. Inode and Device ID
    print(f"Inode:          {st.st_ino}")
    print(f"Device ID:      {st.st_dev}")
    print("-" * 40)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python inspect_file.py <file_path>")
    else:
        inspect_file(sys.argv[1])
