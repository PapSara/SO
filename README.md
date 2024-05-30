File and Directory Snapshot Creation and Isolation

This C program creates snapshots of specified directories, stores file information, and isolates potentially dangerous files. It uses multiple processes to handle directories in parallel, checks file permissions, and moves files without permissions to a designated isolation directory. It also compares current snapshots with previous ones and updates accordingly.
