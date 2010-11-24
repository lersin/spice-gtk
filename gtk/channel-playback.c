/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2010 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#include "spice-client.h"
#include "spice-common.h"
#include "spice-channel-priv.h"

#include "spice-marshal.h"

#define SPICE_PLAYBACK_CHANNEL_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), SPICE_TYPE_PLAYBACK_CHANNEL, spice_playback_channel))

struct spice_playback_channel {
    int                         mode;
};

G_DEFINE_TYPE(SpicePlaybackChannel, spice_playback_channel, SPICE_TYPE_CHANNEL)

enum {
    SPICE_PLAYBACK_START,
    SPICE_PLAYBACK_DATA,
    SPICE_PLAYBACK_STOP,

    SPICE_PLAYBACK_LAST_SIGNAL,
};

static guint signals[SPICE_PLAYBACK_LAST_SIGNAL];

static void spice_playback_handle_msg(SpiceChannel *channel, spice_msg_in *msg);

/* ------------------------------------------------------------------ */

static void spice_playback_channel_init(SpicePlaybackChannel *channel)
{
    spice_playback_channel *c;

    c = channel->priv = SPICE_PLAYBACK_CHANNEL_GET_PRIVATE(channel);
    memset(c, 0, sizeof(*c));
}

static void spice_playback_channel_finalize(GObject *obj)
{
    if (G_OBJECT_CLASS(spice_playback_channel_parent_class)->finalize)
        G_OBJECT_CLASS(spice_playback_channel_parent_class)->finalize(obj);
}

static void spice_playback_channel_class_init(SpicePlaybackChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    SpiceChannelClass *channel_class = SPICE_CHANNEL_CLASS(klass);

    gobject_class->finalize     = spice_playback_channel_finalize;
    channel_class->handle_msg   = spice_playback_handle_msg;

    signals[SPICE_PLAYBACK_START] =
        g_signal_new("playback-start",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpicePlaybackChannelClass, spice_playback_start),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__INT_INT_INT,
                     G_TYPE_NONE,
                     3,
                     G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

    signals[SPICE_PLAYBACK_DATA] =
        g_signal_new("playback-data",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpicePlaybackChannelClass, spice_playback_data),
                     NULL, NULL,
                     g_cclosure_user_marshal_VOID__POINTER_INT,
                     G_TYPE_NONE,
                     2,
                     G_TYPE_POINTER, G_TYPE_INT);

    signals[SPICE_PLAYBACK_STOP] =
        g_signal_new("playback-stop",
                     G_OBJECT_CLASS_TYPE(gobject_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(SpicePlaybackChannelClass, spice_playback_stop),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    g_type_class_add_private(klass, sizeof(spice_playback_channel));
}

/* ------------------------------------------------------------------ */

static void playback_handle_data(SpiceChannel *channel, spice_msg_in *in)
{
    spice_playback_channel *c = SPICE_PLAYBACK_CHANNEL(channel)->priv;
    SpiceMsgPlaybackPacket *data = spice_msg_in_parsed(in);

    SPICE_DEBUG("%s: time %d data %p size %d", __FUNCTION__,
            data->time, data->data, data->data_size);

    switch (c->mode) {
    case SPICE_AUDIO_DATA_MODE_RAW:
        g_signal_emit(channel, signals[SPICE_PLAYBACK_DATA], 0,
                      data->data, data->data_size);
        break;
    default:
        g_warning("%s: unhandled mode", __FUNCTION__);
        break;
    }
}

static void playback_handle_mode(SpiceChannel *channel, spice_msg_in *in)
{
    spice_playback_channel *c = SPICE_PLAYBACK_CHANNEL(channel)->priv;
    SpiceMsgPlaybackMode *mode = spice_msg_in_parsed(in);

    SPICE_DEBUG("%s: time %d mode %d data %p size %d", __FUNCTION__,
            mode->time, mode->mode, mode->data, mode->data_size);

    c->mode = mode->mode;
    switch (c->mode) {
    case SPICE_AUDIO_DATA_MODE_RAW:
        break;
    default:
        g_warning("%s: unhandled mode", __FUNCTION__);
        break;
    }
}

static void playback_handle_start(SpiceChannel *channel, spice_msg_in *in)
{
    spice_playback_channel *c = SPICE_PLAYBACK_CHANNEL(channel)->priv;
    SpiceMsgPlaybackStart *start = spice_msg_in_parsed(in);

    SPICE_DEBUG("%s: fmt %d channels %d freq %d time %d", __FUNCTION__,
            start->format, start->channels, start->frequency, start->time);

    switch (c->mode) {
    case SPICE_AUDIO_DATA_MODE_RAW:
        g_signal_emit(channel, signals[SPICE_PLAYBACK_START], 0,
                      start->format, start->channels, start->frequency);
        break;
    default:
        g_warning("%s: unhandled mode", __FUNCTION__);
        break;
    }
}

static void playback_handle_stop(SpiceChannel *channel, spice_msg_in *in)
{
    g_signal_emit(channel, signals[SPICE_PLAYBACK_STOP], 0);
}

static spice_msg_handler playback_handlers[] = {
    [ SPICE_MSG_SET_ACK ]                  = spice_channel_handle_set_ack,
    [ SPICE_MSG_PING ]                     = spice_channel_handle_ping,
    [ SPICE_MSG_NOTIFY ]                   = spice_channel_handle_notify,
    [ SPICE_MSG_DISCONNECTING ]            = spice_channel_handle_disconnect,
    [ SPICE_MSG_WAIT_FOR_CHANNELS ]        = spice_channel_handle_wait_for_channels,
    [ SPICE_MSG_MIGRATE ]                  = spice_channel_handle_migrate,

    [ SPICE_MSG_PLAYBACK_DATA ]            = playback_handle_data,
    [ SPICE_MSG_PLAYBACK_MODE ]            = playback_handle_mode,
    [ SPICE_MSG_PLAYBACK_START ]           = playback_handle_start,
    [ SPICE_MSG_PLAYBACK_STOP ]            = playback_handle_stop,
};

static void spice_playback_handle_msg(SpiceChannel *channel, spice_msg_in *msg)
{
    int type = spice_msg_in_type(msg);
    g_return_if_fail(type < SPICE_N_ELEMENTS(playback_handlers));
    g_return_if_fail(playback_handlers[type] != NULL);
    playback_handlers[type](channel, msg);
}
