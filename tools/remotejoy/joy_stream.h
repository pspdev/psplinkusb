/* RemoteJoy events are a byte stream, not USB/TCP packet boundaries. */
#ifndef REMOTEJOY_STREAM_H
#define REMOTEJOY_STREAM_H

#include <stdint.h>
#include <stddef.h>
#include "remotejoy.h"

struct JoyStream
{
	uint32_t magic_window;
	unsigned char bytes[sizeof(struct JoyEvent)];
	unsigned int used;
};

static inline uint32_t joy_read_le32(const unsigned char *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Return one decoded event when the last byte arrives. */
static inline int joy_stream_push(struct JoyStream *stream, unsigned char byte,
		struct JoyEvent *event)
{
	/* Look for the next header even after a truncated event. The magic
	 * word is not a valid RemoteJoy event type or controller value. */
	stream->magic_window = (stream->magic_window >> 8) | ((uint32_t)byte << 24);
	if(stream->magic_window == JOY_MAGIC)
	{
		stream->used = offsetof(struct JoyEvent, type);
		return 0;
	}
	if(!stream->used)
	{
		return 0;
	}
	stream->bytes[stream->used++] = byte;
	if(stream->used != sizeof(stream->bytes))
	{
		return 0;
	}
	event->magic = JOY_MAGIC;
	event->type = joy_read_le32(stream->bytes + offsetof(struct JoyEvent, type));
	event->value = joy_read_le32(stream->bytes + offsetof(struct JoyEvent, value));
	stream->used = 0;
	if(event->type < TYPE_BUTTON_DOWN || event->type > TYPE_SCREEN_CMD ||
		((event->type == TYPE_ANALOG_X || event->type == TYPE_ANALOG_Y) && event->value > JOY_ANALOG_MAX))
	{
		return 0;
	}
	return 1;
}

#endif
