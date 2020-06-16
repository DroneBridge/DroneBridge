/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2020 Wolfgang Christl
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <signal.h>

#include "../common/db_common.h"
#include "../common/db_unix.h"

#define MAX_LEN_FILENAME    64
#define MAX_LEN_FILEPATH    512
#define REC_BUFF_SIZE       819200  // ~6Mbit
#define MAX_RETRY_UNIX      100
#define MIN_WRITE_BUFF_LEN  10240   // minimum size of buffered data before fwrite

typedef struct {
    FILE *file_pnt;
    char filepath[MAX_LEN_FILEPATH];
} video_file_t;

volatile int recorder_running = true;

void int_handler(int dummy) {
    recorder_running = false;
}

bool file_exists(char fname[]) {
    if (access(fname, F_OK) != -1)
        return true;
    else
        return false;
}

void open_new_file(char record_dir[MAX_LEN_FILEPATH-MAX_LEN_FILENAME], video_file_t *video_file) {
    char filename[MAX_LEN_FILENAME];
    char filepath[MAX_LEN_FILEPATH - 5];
    struct tm *timenow;

    strcpy(filepath, record_dir);
    time_t now = time(NULL);
    timenow = localtime(&now);
    strftime(filename, sizeof(filename), "/DB_VIDEO_%F_%H-%M", timenow);
    strcat(filepath, filename);
    if (file_exists(filepath)) {
        for (int i = 0; i < 10; i++) {
            char str[12];
            sprintf(str, "_%d", i);
            strcat(filepath, str);
            if (!file_exists(filepath))
                break;
        }
    }
    strcat(filepath, ".264");

    FILE *file_pnter;
    LOG_SYS_STD(LOG_INFO, "DB_RECORDER: Writing video data to: %s\n", filepath);
    file_pnter = fopen(filepath, "wb");
    if (file_pnter == NULL)
        LOG_SYS_STD(LOG_ERR, "DB_RECORDER: Could not open video file > %s\n", strerror(errno));
    video_file->file_pnt = file_pnter;
    strcpy(video_file->filepath, filepath);
}

void delete_file(video_file_t *video_file) {
    fclose(video_file->file_pnt);
    if (remove(video_file->filepath) != 0)
        LOG_SYS_STD(LOG_INFO, "DB_RECORDER: Unable to delete empty file %s > %s!\n", video_file->filepath, strerror(errno));
}

bool write_to_file(video_file_t *video_file, uint8_t *rec_buff, size_t num_new_bytes) {
    if ((*video_file).file_pnt != NULL) {
        ssize_t written = fwrite(rec_buff, (int) num_new_bytes, 1, (*video_file).file_pnt);
        if (written != 1) {
            LOG_SYS_STD(LOG_WARNING, "DB_RECORDER: Not all data written to file (%ld/%ld)\n", written, num_new_bytes);
        } else
            return true;
    }
    return false;
}

/**
 * Connects to UNIX domain socket of video air module to receive raw H.264 video data. Writes data to file.
 * Takes directory to save recorded files as input argument
 *
 * Example usage:
 * ./recorder /DroneBridge/recordings
 */
int main(int argc, char *argv[]) {
    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);
    signal(SIGKILL, int_handler);

    char record_dir[MAX_LEN_FILEPATH-MAX_LEN_FILENAME];
    if (strlen(argv[1]) == 0)
        strcpy(record_dir, "/DroneBridge/recordings");
    else
        strcpy(record_dir, argv[1]);
    video_file_t video_file;
    open_new_file(record_dir, &video_file);
    if (video_file.file_pnt == NULL) {
        return -1;
    }

    int domain_sock;
    struct sockaddr_un address;
    if((domain_sock = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        LOG_SYS_STD(LOG_ERR, "DB_RECORDER: Could not open domain socket > %s\n", strerror(errno));
        delete_file(&video_file);
        return -1;
    }
    address.sun_family = AF_LOCAL;
    strcpy(address.sun_path, DB_UNIX_DOMAIN_VIDEO_PATH);
    int retry_cnt = 0;
    bool unix_connected = false;
    do {
        if (connect(domain_sock, (struct sockaddr *) &address, sizeof (address)) == 0) {
            LOG_SYS_STD(LOG_INFO, "DB_RECORDER: Connected to %s\n", DB_UNIX_DOMAIN_VIDEO_PATH);
            unix_connected = true;
        } else {
            usleep(50000);
            retry_cnt++;
        }
    } while (retry_cnt != MAX_RETRY_UNIX && !unix_connected);
    if (!unix_connected) {
        LOG_SYS_STD(LOG_INFO, "DB_RECORDER: Could not connect to %s > %s\n", DB_UNIX_DOMAIN_VIDEO_PATH, strerror(errno));
        delete_file(&video_file);
        close(domain_sock);
        return -1;
    }

    fd_set fd_socket_set;
    struct timeval select_timeout;
    uint8_t rec_buff[REC_BUFF_SIZE];
    ssize_t len_buffered = 0;
    while (recorder_running) {
        // select with timeout to be able to safely kill the recorder writing all remaining data
        select_timeout.tv_sec = 1;
        select_timeout.tv_usec = 0;
        FD_ZERO(&fd_socket_set);
        FD_SET(domain_sock, &fd_socket_set);
        int select_return = select(domain_sock + 1, &fd_socket_set, NULL, NULL, &select_timeout);
        if (select_return > 0) {
            if (FD_ISSET(domain_sock, &fd_socket_set)) {
                len_buffered += recv(domain_sock, &rec_buff[len_buffered], REC_BUFF_SIZE - len_buffered, 0);
                if (len_buffered >= MIN_WRITE_BUFF_LEN) {
                    if (write_to_file(&video_file, rec_buff, len_buffered))
                        len_buffered = 0;
                }
            }
        } else if(select_return == -1) {
            if (errno != EINTR) // ignore kill event
                LOG_SYS_STD(LOG_WARNING, "DB_RECORDER: Select returned an error > %s\n", strerror(errno));
        }
    }
    write_to_file(&video_file, rec_buff, len_buffered);
    close(domain_sock);
    fclose(video_file.file_pnt);
    LOG_SYS_STD(LOG_INFO, "DB_RECORDER: Terminated!\n");
    return 0;
}