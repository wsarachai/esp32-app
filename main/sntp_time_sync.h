/*
 * sntp_time_sync.h
 *
 *  Created on: May 13, 2024
 *      Author: keng
 */

#ifndef MAIN_SNTP_TIME_SYNC_H_
#define MAIN_SNTP_TIME_SYNC_H_

/**
 * Starts the NTP server synchronization task.
 */
void sntp_time_sync_task_start(void);

/**
 * Returns local time if set.
 * @return local time buffer.
 */
char* sntp_time_sync_get_time(void);

#endif /* MAIN_SNTP_TIME_SYNC_H_ */
