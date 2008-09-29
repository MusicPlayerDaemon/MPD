/*
 * This file only contains ob_send() and private functions
 * needed to implement ob_send()
 */

/*
 * This is one of two places where dc.thread can block,
 * the other is readFromInputStream
 */
static enum dc_action await_buffer_space(void)
{
	assert(pthread_equal(pthread_self(), dc.thread));
	/* DEBUG("Waiting for buffer space\n"); */
	assert(dc.state != DC_STATE_STOP);

	dc_halt();
	/* DEBUG("done waiting for buffer space\n"); */
	return dc.action;
}

/* This will modify its input */
static void do_audio_conversion(void **data, size_t *len)
{
	size_t newlen;

	assert(pthread_equal(pthread_self(), dc.thread));
	newlen = pcm_sizeOfConvBuffer(&dc.audio_format, *len, &ob.audio_format);
	if (newlen > ob.conv_buf_len) {
		ob.conv_buf = xrealloc(ob.conv_buf, newlen);
		ob.conv_buf_len = newlen;
	}
	*len = pcm_convertAudioFormat(&dc.audio_format, *data, *len,
	                              &ob.audio_format, ob.conv_buf,
	                              &ob.conv_state);
	*data = ob.conv_buf;
}

static void ensure_audio_format_sanity(void **data, size_t *len)
{
	static uint8_t seq_last;

	assert(pthread_equal(pthread_self(), dc.thread));
	if (mpd_unlikely(seq_last != ob.seq_decoder)) {
		seq_last = ob.seq_decoder;
		if (cmpAudioFormat(&dc.audio_format, &ob.audio_format))
			getOutputAudioFormat(&dc.audio_format,
			                     &ob.audio_format);
	}
	if (cmpAudioFormat(&ob.audio_format, &dc.audio_format))
		do_audio_conversion(data, len);
}

static void start_playback(void)
{
	assert(pthread_equal(pthread_self(), dc.thread));
	if (mpd_unlikely(ob.state == OB_STATE_STOP &&
	                 player_errno == PLAYER_ERROR_NONE)) {
		ob_trigger_action(OB_ACTION_PLAY);
	}
}

enum dc_action
ob_send(void *data, size_t len,
        float decode_time, uint16_t bit_rate, ReplayGainInfo * rgi)
{
	struct rbvec vec[2];
	struct ob_chunk *c;
	size_t idx;
	size_t i, j;

	assert(pthread_equal(pthread_self(), dc.thread));

	ensure_audio_format_sanity(&data, &len);

	if (rgi && (replayGainState != REPLAYGAIN_OFF))
		doReplayGain(rgi, data, len, &ob.audio_format);
	else if (normalizationEnabled)
		normalizeData(data, len, &ob.audio_format);

	while (1) {
		/* full buffer, loop check in case of spurious wakeups */
		while (!ringbuf_get_write_vector(ob.index, vec)) {
			enum dc_action rv = await_buffer_space();
			if (rv != DC_ACTION_NONE)
				return rv;
		}

		for (i = 0; i < ARRAY_SIZE(vec); i++) {
			for (j = 0; j < vec[i].len; j++) {
				size_t c_len;
				idx = vec[i].base - ob.index->buf + j;
				c = &(ob.chunks[idx]);

				if (!c->len) { /* populate empty chunk */
					c->seq = ob.seq_decoder;
					c->time = decode_time;
					c->bit_rate = bit_rate;
					c_len = len > CHUNK_SIZE ? CHUNK_SIZE
					                         : len;
					c->len = (uint16_t)c_len;
					memcpy(c->data, data, c_len);
				} else { /* partially filled chunk */
					size_t max = CHUNK_SIZE - c->len;
					assert(c->seq == ob.seq_decoder);
					c_len = len > max ? max : len;
					assert(c_len <= CHUNK_SIZE);
					memcpy(c->data + c->len, data, c_len);
					c->len += c_len;
					assert(c->len <= CHUNK_SIZE);
				}

				/*
				 * feed ob.thread ASAP, otherwise ob.thread
				 * will just play silence
				 */
				if (c->len == CHUNK_SIZE)
					ringbuf_write_advance(ob.index, 1);

				assert(len >= c_len);
				len -= c_len;
				if (!len) {
					start_playback();
					return dc.action;
				}
				data = (unsigned char *)data + c_len;
			}
		}
	}
	assert(__FILE__ && __LINE__ && "We should never get here" && 0);
	return DC_ACTION_NONE;
}


