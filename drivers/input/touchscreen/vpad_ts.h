#ifndef VPAD_TS_H_
#define VPAD_TS_H_

#define Unidisplay_7inch

#ifdef NAS_7inch
#define TOUCHSCREEN_MINX 0
#define TOUCHSCREEN_MAXX 1791
#define TOUCHSCREEN_MINY 0
#define TOUCHSCREEN_MAXY 1023

#elif defined MUTTO_7inch
#define TOUCHSCREEN_MINX 0
#define TOUCHSCREEN_MAXX 799
#define TOUCHSCREEN_MINY 0
#define TOUCHSCREEN_MAXY 479

#elif defined Unidisplay_7inch
#define TOUCHSCREEN_MINX 0
#define TOUCHSCREEN_MAXX 3968
#define TOUCHSCREEN_MINY 0
#define TOUCHSCREEN_MAXY 2304

/* Unidisplay_9_7inch */
#else
#define TOUCHSCREEN_MINX 0
#define TOUCHSCREEN_MAXX 4351
#define TOUCHSCREEN_MINY 0
#define TOUCHSCREEN_MAXY 3327
#endif

#endif /* VPAD_TS_H_ */
