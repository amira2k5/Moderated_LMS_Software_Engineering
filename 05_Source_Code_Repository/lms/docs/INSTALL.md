# Installation guide (for the school technician)

Time needed: about 30 minutes. The program needs no internet connection after it has been built.

## What you need
* One school computer with Ubuntu 22.04 or later (or Windows 10 with WSL2 and Ubuntu). 2 GB of memory is enough.
* The program folder (`lms`) from the release, or the source code.
* A folder with at least 1 GB of free space for the database and the students' files.

## Steps
1. Install the tools (needs internet once): `sudo apt-get install g++ cmake libsqlite3-dev libsodium-dev`.
2. Open a terminal in the program folder and build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLMS_BUILD_TESTS=OFF`
   then `cmake --build build`.
3. Copy `build/lms` to a permanent folder, for example `/opt/school-lms/`. In the same folder create two places:
   the database file `lms.db` (created automatically) and the folder `lms_files`.
4. Start the program: `cd /opt/school-lms && ./lms --db lms.db --files lms_files`.
5. On the first start the program asks for the first administrator account. Write the details in the school's safe place.
6. Check the installation: log in, create a department, then log out. If you can do this, the installation works.

## Daily use on a local network
Install on one computer and let staff and students use that computer in turn, or share the folder over the school
network so that each person runs the program on their own computer against the same `lms.db`. Up to 5 people at the
same time are supported; if the database is busy the program waits a few seconds and then shows a message.

## Backups (do this every week and before every update)
1. Log in as the administrator and choose **Back up the database**. Type a file name such as `backup-2026-11-06.db`.
2. Copy the backup file and the `lms_files` folder to a USB drive or another computer.

## Restoring a backup (about 10 minutes)
1. Close the program. Rename the current `lms.db` to `lms.db.broken`.
2. Copy the backup file to `lms.db` in the program folder and copy the saved `lms_files` folder back.
3. Start the program and log in. Check that a recent assignment and its marks are there.

## Updating
Make a backup first. Replace the `lms` program file with the new one. The database is kept as it is.

## Problems
| Message | What to do |
|---|---|
| The database file could not be opened | Check the folder name and that you have permission to write there. |
| The database reported a problem: database is locked | Someone else is saving. Wait a minute and try again. |
| The file could not be saved | The `lms_files` folder is missing or full. Create it or free some space. |
