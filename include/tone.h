#ifndef TONE_H
#define TONE_H

/* 
 * These are 10 periods of the 425 Hz tone used by most inband signals
 * Its actually not exacly 425 Hz, but 416,66667, which fit very well
 * the 15% tolerance
 */

#define	TONE_425_SIZE		192
extern const unsigned char tone_425[TONE_425_SIZE];

/* 
 * These are 10 ms of silence
 */

#define TONE_SILENCE_SIZE	80
extern const unsigned char tone_SILENCE[TONE_SILENCE_SIZE];

/* Values for tone pattern */
#define TONE_ALERT_SILENCE_TIME	4000000
#define TONE_ALERT_TIME		1000000
#define TONE_BUSY_SILENCE_TIME	500000
#define TONE_BUSY_TIME		500000

extern int		tone_handler(bchannel_t *bc);
extern int		set_tone(bchannel_t *bc, int tone);

#endif
