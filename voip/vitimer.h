#ifndef VITIMER_H
#define VITIMER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

typedef	struct _vi_timer		vi_timer_t;
typedef int (*timef_t)(void *, unsigned long, struct timeval *);

struct _vi_timer {
	vi_timer_t	*prev;
	vi_timer_t	*next;
	struct timeval	tv;
	void		*data;
	unsigned long	val;
	timef_t		func;	
};

extern	int		run_vitimer(void);
extern	void		remove_vitimer(vi_timer_t *);
extern	int		init_vitimer(vi_timer_t *, void *, unsigned long, timef_t);
extern	int		add_vitimer_abs(vi_timer_t *, struct timeval *);
extern	int		add_vitimer_rel(vi_timer_t *, struct timeval *);
extern	void		clean_vitimer(void);
extern	struct timeval	*get_next_vitimer_time(void);
extern	int		get_next_vitimer_dist(struct timeval *);


#endif
