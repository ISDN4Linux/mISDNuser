#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include "vitimer.h"

static	vi_timer_t	*timerlist = NULL;

static void
ins_vitimer(vi_timer_t *iti) {
	iti->prev = timerlist;

	iti->next = NULL;
	if (timerlist) {
		if (timercmp(&timerlist->tv, &iti->tv, >)) {
			iti->prev = NULL;
			iti->next = timerlist;
		}
	}
	while(iti->prev && iti->prev->next) {
		if (timercmp(&iti->prev->next->tv, &iti->tv, >))
			break;
		iti->prev = iti->prev->next;
	}
	if (iti->prev) {
		iti->next = iti->prev->next;
		iti->prev->next = iti;
	} else {
		timerlist = iti;
	}
	if (iti->next)
		iti->next->prev = iti;
}

int
run_vitimer(void)
{
	int	cnt = 0;
	struct timeval	act, del;

	gettimeofday(&act, NULL);
	while(timerlist) {
		if (timercmp(&timerlist->tv, &act, >))
			break;
		timersub(&act, &timerlist->tv, &del);
		timerlist->func(timerlist->data, timerlist->val, &del);
		remove_vitimer(timerlist);
	}
	return(cnt);
}

void
remove_vitimer(vi_timer_t *iti) {
	if (iti->prev)
		iti->prev->next = iti->next;
	if (iti->next)
		iti->next->prev = iti->prev;
	if (timerlist == iti)
		timerlist = iti->next;
	iti->prev = NULL;
	iti->next = NULL;
}

int
init_vitimer(vi_timer_t *iti, void *data, unsigned long val, timef_t f)
{
	if (!iti) {
		return(-EINVAL);
	}
	iti->data = data;
	iti->val = val;
	iti->func = f;
	return(0);
}

int
add_vitimer_abs(vi_timer_t *iti, struct timeval *tv)
{
	if (!iti) {
		return(-EINVAL);
	}
	iti->tv = *tv;
	ins_vitimer(iti);
	run_vitimer();
	return(0);
}


int
add_vitimer_rel(vi_timer_t *iti, struct timeval *tv)
{
	struct timeval	act;

	gettimeofday(&act, NULL);
	if (!iti) {
		return(-EINVAL);
	}
	timeradd(&act, tv, &iti->tv);
	ins_vitimer(iti);
	run_vitimer();
	return(0);
}


void
clean_vitimer(void) {
	while(timerlist)
		remove_vitimer(timerlist);
}

struct timeval
*get_next_vitimer_time(void)
{
	if (timerlist)
		return(&timerlist->tv);
	else
		return(NULL);
}

int
get_next_vitimer_dist(struct timeval *tv)
{
	struct timeval	act;

	if (!tv)
		return(-EINVAL);
	if (!timerlist)
		return(-EINVAL);
	gettimeofday(&act, NULL);
	if (timercmp(&timerlist->tv, &act, <)) {
		tv->tv_sec = 0;
		tv->tv_usec = 0;
	} else
		timersub(&timerlist->tv, &act, tv);
	return(0);
}
