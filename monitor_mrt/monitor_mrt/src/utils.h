#ifndef UTILS_H
#define UTILS_H

#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdint.h>

void GetTimestamp(char *currentTime);
int GetMillis();

char timestamp[100];

char *inet_fmt(uint32_t addr, char *s, size_t len);
uint32_t inet_parse(char *s, int n);

#endif