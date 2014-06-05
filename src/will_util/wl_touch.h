#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <linux/input.h>

//#define DEBUG
//#define TOUCH_USE_SELECT

typedef struct {
	int x;
	int y;
} point_t;

#define TOUCH_POINT_MAX 1
#define TOUCH_PATH_MAX 1

typedef struct {
	int touch;
	int double_tap;
	// currently just 2 slots are supported
	point_t paths[TOUCH_PATH_MAX + 1][TOUCH_POINT_MAX + 1];
	struct timeval time;
} touch_data_t;

static void touch_data_reset(touch_data_t *td) {
	td->touch = -1;
	td->double_tap = -1;
	int i, j;
	for (i = 0; i <= TOUCH_PATH_MAX; i++) {
		for (j = 0; j <= TOUCH_POINT_MAX; j++) {
			td->paths[i][j].x = -1;
			td->paths[i][j].y = -1;
		}
	}
}

#ifdef DEBUG
static int touch_event_id = 0;
#endif
static int touch_input_fd = -1;
static struct input_event touch_event;
static int touch_event_size = sizeof(struct input_event);
static int touch_slot_id = -1;
static touch_data_t touch_data;

static int touch_reader_quit = 0;
static pthread_t touch_reader_thread;

typedef void (*touch_callback_t)(touch_data_t *data);
static touch_callback_t touch_callback = NULL;

void touch_reader_free() {
	if (touch_input_fd != -1) {
		touch_reader_quit = 1;
		pthread_join(touch_reader_thread, NULL);
		close(touch_input_fd);
		touch_input_fd = -1;
	}
}

static int touch_reader_read() {
	if (read(touch_input_fd, &touch_event, touch_event_size)
			!= touch_event_size) {
		printf("error: read()\n");
		return 0;
	}

	switch (touch_event.type) {
	case EV_KEY:
		if (touch_event.code == BTN_TOUCH) {
#ifdef DEBUG
			printf("[%d] KEY-BTN_TOUCH-%d\n", ++touch_event_id,
					touch_event.value);
#endif
			touch_data.touch = touch_event.value;
		} else if (touch_event.code == BTN_TOOL_DOUBLETAP) {
#ifdef DEBUG
			printf("[%d] KEY-BTN_TOOL_DOUBLETAP-%d\n", ++touch_event_id,
					touch_event.value);
#endif
			touch_data.double_tap = touch_event.value;
		} else {
#ifdef DEBUG
			if (touch_event.code == BTN_TOOL_FINGER) {
				printf("[%d] KEY-BTN_TOOL_FINGER-%d\n", ++touch_event_id,
						touch_event.value);
			} else {
				printf("[%d] KEY-0x%x-0x%x\n", ++touch_event_id,
						touch_event.code, touch_event.value);
			}
#endif
		}
		break;
	case EV_ABS:
		if (touch_event.code == ABS_MT_POSITION_X) {
			if (0 <= touch_slot_id && touch_slot_id <= TOUCH_PATH_MAX) {
				if (touch_data.paths[touch_slot_id][0].x == -1)
					touch_data.paths[touch_slot_id][0].x = touch_event.value;
				else
					touch_data.paths[touch_slot_id][TOUCH_POINT_MAX].x =
							touch_event.value;
			}
#ifdef DEBUG
			printf("[%d] ABS-X-%d\n", ++touch_event_id, touch_event.value);
#endif
		} else if (touch_event.code == ABS_MT_POSITION_Y) {
			if (0 <= touch_slot_id && touch_slot_id <= TOUCH_PATH_MAX) {
				if (touch_data.paths[touch_slot_id][0].y == -1)
					touch_data.paths[touch_slot_id][0].y = touch_event.value;
				else
					touch_data.paths[touch_slot_id][TOUCH_POINT_MAX].y =
							touch_event.value;
			}
#ifdef DEBUG
			printf("[%d] ABS-Y-%d\n", ++touch_event_id, touch_event.value);
#endif
		} else if (touch_event.code == ABS_MT_TRACKING_ID) {
			touch_slot_id = touch_event.value;
#ifdef DEBUG
			printf("[%d] ABS-TRACKING_ID-%d\n", ++touch_event_id,
					touch_event.value);
#endif
		} else if (touch_event.code == ABS_MT_SLOT) {
			touch_slot_id = touch_event.value;
#ifdef DEBUG
			printf("[%d] ABS-SLOT-%d\n", ++touch_event_id, touch_event.value);
#endif
		} else {
#ifdef DEBUG
			printf("[%d] ABS-0x%x-0x%x\n", ++touch_event_id, touch_event.code,
					touch_event.value);
#endif
		}
		break;
	case EV_SYN:
#ifdef DEBUG
		if (touch_event.code == SYN_REPORT && touch_event.value == 0)
		printf("[%d] --SYN--\n\n", ++touch_event_id);
		else
		printf("[%d] SYN-0x%x-0x%x\n", ++touch_event_id, touch_event.code,
				touch_event.value);
#endif
		if (touch_data.touch == 0) { // a touch event ended
#ifdef DEBUG
				int p = TOUCH_POINT_MAX;
				printf("[TOUCH] slot 1: (%d,%d)/(%d,%d), slot 2: (%d,%d)/(%d,%d)\n",
						touch_data.paths[0][0].x, touch_data.paths[0][0].y,
						touch_data.paths[0][p].x, touch_data.paths[0][p].y,
						touch_data.paths[1][0].x, touch_data.paths[1][0].y,
						touch_data.paths[1][p].x, touch_data.paths[1][p].y);
#endif
//				if (in.double_tap == -1) { // single tap
//				} else if (in.double_tap == 0) { // a double tap event ended
//					// double tap event
//				} else if (in.double_tap == 1) { // a double tap event started but not ended
//					// wait for the double tap event to end
//				}
			if (touch_callback) {
				touch_data.time = touch_event.time;
				touch_callback(&touch_data);
			}
			touch_data_reset(&touch_data);
			touch_slot_id = -1;
		} else if (touch_data.touch == 1) { // a touch event started but not ended
			// wait for the touch event to end
		}
		break;
	default:
#ifdef DEBUG
		printf("[%d] 0x%x-0x%x-0x%x\n", ++touch_event_id, touch_event.type,
				touch_event.code, touch_event.value);
#endif
		break;
	}
	return 1;
}

// https://www.kernel.org/doc/Documentation/input/input.txt
// https://www.kernel.org/doc/Documentation/input/multi-touch-protocol.txt
// http://linux.die.net/man/2/open
// http://stackoverflow.com/questions/20943322/accessing-keys-from-linux-input-device
static void *touch_reader_run(void *args) {
	if (args)
		printf("touch_reader_run: args");

	if (touch_input_fd == -1) {
		printf("error: touch_reader_fd == -1\n");
		return NULL;
	}

#ifdef TOUCH_USE_SELECT
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(touch_input_fd, &readfds);
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = 1;
	int rc;
#endif
	while (!touch_reader_quit) {
#ifdef TOUCH_USE_SELECT
		rc = select(touch_input_fd + 1, &readfds, NULL, NULL, &tv);
		if (rc == -1) {
			printf("error: select()\n");
			break;
		}
		if (rc == 0) // timeout
			continue;
		if (!FD_ISSET(touch_input_fd, &readfds))
			continue;
#endif
		touch_reader_read();
	}
	printf("[DEBUG] quit touch reader\n");
	return NULL;
}

static int touch_reader_init(touch_callback_t callback, int start_thread) {
	touch_callback = callback;
	touch_data_reset(&touch_data);
	const char *dev = "/dev/input/event1";
	touch_input_fd = open(dev, O_RDONLY); // | O_NONBLOCK);
	if (touch_input_fd == -1) {
		printf("error: open()\n");
		return 0;
	}
	if (start_thread) {
		if (pthread_create(&touch_reader_thread, NULL, touch_reader_run, NULL)) {
			printf("error: pthread_create()\n");
			return 0;
		}
	}
	printf("[TOUCH] input device: %s\n", dev);
	return 1;
}

static inline double distance_of(point_t *p1, point_t *p2) {
	int x = p1->x - p2->x;
	int y = p1->y - p2->y;
	return sqrt(x * x + y * y);
}

//typedef enum touch_type {
//	SINGLE_TAP,
//	DOUBLE_TAP,
//	SWIP
//} touch_type;

//typedef struct {
////	touch_type type;
//	int tap_subjects;
//	int point_count;
//	point *points;
//} touch_event;
