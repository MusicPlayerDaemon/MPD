/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "raop_output_plugin.h"
#include "output_api.h"
#include "mixer_list.h"
#include "raop_output_plugin.h"
#include "rtsp_client.h"
#include "glib_compat.h"

#include <glib.h>
#include <unistd.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>

#ifndef WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "raop"

static struct raop_session_data *raop_session = NULL;

/**
 * The quark used for GError.domain.
 */
static inline GQuark
raop_output_quark(void)
{
	return g_quark_from_static_string("raop_output");
}

static struct raop_data *
new_raop_data(GError **error_r)
{
	struct raop_data *ret = g_new(struct raop_data, 1);
	int i;

	ret->control_mutex = g_mutex_new();

	ret->next = NULL;
	ret->is_master = 0;
	ret->started = 0;
	ret->paused = 0;

	if (raop_session == NULL) {
		raop_session = g_new(struct raop_session_data, 1);
		raop_session->raop_list = NULL;
		ntp_server_init(&raop_session->ntp);
		raop_session->ctrl.port = 6001;
		raop_session->ctrl.fd = -1;
		raop_session->play_state.playing = false;
		raop_session->play_state.seq_num = (short) g_random_int();
		raop_session->play_state.rtptime = g_random_int();
		raop_session->play_state.sync_src = g_random_int();
		raop_session->play_state.last_send.tv_sec = 0;
		raop_session->play_state.last_send.tv_usec = 0;

		if (!RAND_bytes(raop_session->encrypt.iv, sizeof(raop_session->encrypt.iv)) || !RAND_bytes(raop_session->encrypt.key, sizeof(raop_session->encrypt.key))) {
			g_set_error(error_r, raop_output_quark(), 0,
				    "RAND_bytes error code=%ld", ERR_get_error());
			return NULL;
		}
		memcpy(raop_session->encrypt.nv, raop_session->encrypt.iv, sizeof(raop_session->encrypt.nv));
		for (i = 0; i < 16; i++) {
			printf("0x%x ", raop_session->encrypt.key[i]);
		}
		printf("\n");
		AES_set_encrypt_key(raop_session->encrypt.key, 128, &raop_session->encrypt.ctx);

		raop_session->data_fd = -1;
		memset(raop_session->buffer, 0, RAOP_BUFFER_SIZE);
		raop_session->bufferSize = 0;

		raop_session->data_mutex = g_mutex_new();
		raop_session->list_mutex = g_mutex_new();
	}

	return ret;
}

/*
 * remove one character from a string
 * return the number of deleted characters
 */
static int
remove_char_from_string(char *str, char c)
{
	char *src, *dst;

	/* skip all characters that don't need to be copied */
	src = strchr(str, c);
	if (!src)
		return 0;

	for (dst = src; *src; src++)
		if (*src != c)
			*(dst++) = *src;

	*dst = '\0';

	return src - dst;
}

/* bind an opened socket to specified hostname and port.
 * if hostname=NULL, use INADDR_ANY.
 * if *port=0, use dynamically assigned port
 */
static int bind_host(int sd, char *hostname, unsigned long ulAddr,
		     unsigned short *port, GError **error_r)
{
	struct sockaddr_in my_addr;
	socklen_t nlen = sizeof(struct sockaddr);
	struct hostent *h;

	memset(&my_addr, 0, sizeof(my_addr));
	/* use specified hostname */
	if (hostname) {
		/* get server IP address (no check if input is IP address or DNS name) */
		h = gethostbyname(hostname);
		if (h == NULL) {
			if (strstr(hostname, "255.255.255.255") == hostname) {
				my_addr.sin_addr.s_addr=-1;
			} else {
				if ((my_addr.sin_addr.s_addr = inet_addr(hostname)) == 0xFFFFFFFF) {
					g_set_error(error_r, raop_output_quark(), 0,
						    "failed to resolve host '%s'",
						    hostname);
					return -1;
				}
			}
			my_addr.sin_family = AF_INET;
		} else {
			my_addr.sin_family = h->h_addrtype;
			memcpy((char *) &my_addr.sin_addr.s_addr,
			       h->h_addr_list[0], h->h_length);
		}
	} else {
		// if hostname=NULL, use INADDR_ANY
		if (ulAddr)
			my_addr.sin_addr.s_addr = ulAddr;
		else
			my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		my_addr.sin_family = AF_INET;
	}

	/* bind a specified port */
	my_addr.sin_port = htons(*port);

	if (bind(sd, (struct sockaddr *) &my_addr, sizeof(my_addr)) < 0) {
		g_set_error(error_r, raop_output_quark(), errno,
			    "failed to bind socket: %s",
			    g_strerror(errno));
		return -1;
	}

	if (*port == 0) {
		getsockname(sd, (struct sockaddr *) &my_addr, &nlen);
		*port = ntohs(my_addr.sin_port);
	}

	return 0;
}

/*
 * open udp  port
 */
static int
open_udp_socket(char *hostname, unsigned short *port,
		GError **error_r)
{
	int sd;
	int size = 30000;

	/* socket creation */
	sd = socket(PF_INET, SOCK_DGRAM, 0);
	if (sd < 0) {
		g_set_error(error_r, raop_output_quark(), errno,
			    "failed to create UDP socket: %s",
			    g_strerror(errno));
		return -1;
	}
	if (setsockopt(sd, SOL_SOCKET, SO_SNDBUF, (void *) &size, sizeof(size)) < 0) {
		g_set_error(error_r, raop_output_quark(), errno,
			    "failed to set UDP buffer size: %s",
			    g_strerror(errno));
		return -1;
	}
	if (bind_host(sd, hostname, 0, port, error_r)) {
		close(sd);
		return -1;
	}

	return sd;
}

static bool
get_sockaddr_by_host(const char *host, short destport,
		     struct sockaddr_in *addr,
		     GError **error_r)
{
	struct hostent *h;

	h = gethostbyname(host);
	if (h) {
		addr->sin_family = h->h_addrtype;
		memcpy((char *) &addr->sin_addr.s_addr, h->h_addr_list[0], h->h_length);
	} else {
		addr->sin_family = AF_INET;
		if ((addr->sin_addr.s_addr=inet_addr(host))==0xFFFFFFFF) {
			g_set_error(error_r, rtsp_client_quark(), 0,
				    "failed to resolve host '%s'", host);
			return false;
		}
	}
	addr->sin_port = htons(destport);
	return true;
}

/*
 * Calculate the current NTP time, store it in the buffer.
 */
static void
fill_int(unsigned char *buffer, uint32_t value)
{
	uint32_t be = GINT32_TO_BE(value);
	memcpy(buffer, &be, sizeof(be));
}

/*
 * Store time in the NTP format in the buffer
 */
static void
fill_time_buffer_with_time(unsigned char *buffer, struct timeval *tout)
{
	unsigned long secs_to_baseline = 964697997;
	double fraction;
	unsigned long long_fraction;
	unsigned long secs;

	fraction = ((double) tout->tv_usec) / 1000000.0;
	long_fraction = (unsigned long) (fraction * 256.0 * 256.0 * 256.0 * 256.0);
	secs = secs_to_baseline + tout->tv_sec;
	fill_int(buffer, secs);
	fill_int(buffer + 4, long_fraction);
}

static void
get_time_for_rtp(struct play_state *state, struct timeval *tout)
{
	unsigned long rtp_diff = state->rtptime - state->start_rtptime;
	unsigned long add_secs = rtp_diff / 44100;
	unsigned long add_usecs = (((rtp_diff % 44100) * 10000) / 441) % 1000000;
	tout->tv_sec = state->start_time.tv_sec + add_secs;
	tout->tv_usec = state->start_time.tv_usec + add_usecs;
	if (tout->tv_usec >= 1000000) {
		tout->tv_sec++;
		tout->tv_usec = tout->tv_usec % 1000000;
	}
}

/*
 * Send a control command
 */
static bool
send_control_command(struct control_data *ctrl, struct raop_data *rd,
		     struct play_state *state,
		     GError **error_r)
{
	unsigned char buf[20];
	int diff;
	int num_bytes;
	struct timeval ctrl_time;

	diff = 88200;
	if (rd->started) {
		buf[0] = 0x80;
		diff += NUMSAMPLES;
	} else {
		buf[0] = 0x90;
		state->playing = true;
		state->start_rtptime = state->rtptime;
	}
	buf[1] = 0xd4;
	buf[2] = 0x00;
	buf[3] = 0x07;
	fill_int(buf + 4, state->rtptime - diff);
	get_time_for_rtp(state, &ctrl_time);
	fill_time_buffer_with_time(buf + 8, &ctrl_time);
	fill_int(buf + 16, state->rtptime);

	num_bytes = sendto(ctrl->fd, buf, sizeof(buf), 0, (struct sockaddr *) &rd->ctrl_addr, sizeof(rd->ctrl_addr));
	if (num_bytes < 0) {
		g_set_error(error_r, raop_output_quark(), errno,
			    "Unable to send control command: %s",
			    g_strerror(errno));
		return false;
	}

	return true;
}

static int rsa_encrypt(const unsigned char *text, int len, unsigned char *res)
{
	RSA *rsa;
	gsize usize;
	unsigned char *modulus;
	unsigned char *exponent;
	int size;

	char n[] =
		"59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUtwC"
		"5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDR"
		"KSKv6kDqnw4UwPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuB"
		"OitnZ/bDzPHrTOZz0Dew0uowxf/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJ"
		"Q+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/UAaHqn9JdsBWLUEpVviYnh"
		"imNVvYFZeCXg/IdTQ+x4IRdiXNv5hEew==";
	char e[] = "AQAB";

	rsa = RSA_new();

	modulus = g_base64_decode(n, &usize);
	rsa->n = BN_bin2bn(modulus, usize, NULL);
	exponent = g_base64_decode(e, &usize);
	rsa->e = BN_bin2bn(exponent, usize, NULL);
	g_free(modulus);
	g_free(exponent);
	size = RSA_public_encrypt(len, text, res, rsa, RSA_PKCS1_OAEP_PADDING);

	RSA_free(rsa);
	return size;
}

static int
raop_encrypt(struct encrypt_data *encryp, unsigned char *data, int size)
{
	// any bytes that fall beyond the last 16 byte page should be sent
	// in the clear
	int alt_size = size - (size % 16);

	memcpy(encryp->nv, encryp->iv, 16);

	AES_cbc_encrypt(data, data, alt_size, &encryp->ctx, encryp->nv, 1);

	return size;
}

/* write bits filed data, *bpos=0 for msb, *bpos=7 for lsb
   d=data, blen=length of bits field
*/
static inline void
bits_write(unsigned char **p, unsigned char d, int blen, int *bpos)
{
	int lb, rb, bd;
	lb =7 - *bpos;
	rb = lb - blen + 1;
	if (rb >= 0) {
		bd = d << rb;
		if (*bpos)
			**p |= bd;
		else
			**p = bd;
		*bpos += blen;
	} else {
		bd = d >> -rb;
		**p |= bd;
		*p += 1;
		**p = d << (8 + rb);
		*bpos = -rb;
	}
}

static bool
wrap_pcm(unsigned char *buffer, int bsize, int *size, unsigned char *inData, int inSize)
{
	unsigned char one[4];
	int count = 0;
	int bpos = 0;
	unsigned char *bp = buffer;
	int i, nodata = 0;
	bits_write(&bp, 1, 3, &bpos); // channel=1, stereo
	bits_write(&bp, 0, 4, &bpos); // unknown
	bits_write(&bp, 0, 8, &bpos); // unknown
	bits_write(&bp, 0, 4, &bpos); // unknown
	if (bsize != 4096 && false)
		bits_write(&bp, 1, 1, &bpos); // hassize
	else
		bits_write(&bp, 0, 1, &bpos); // hassize
	bits_write(&bp, 0, 2, &bpos); // unused
	bits_write(&bp, 1, 1, &bpos); // is-not-compressed
	if (bsize != 4096 && false) {
		// size of data, integer, big endian
		bits_write(&bp, (bsize >> 24) & 0xff, 8, &bpos);
		bits_write(&bp, (bsize >> 16) & 0xff, 8, &bpos);
		bits_write(&bp, (bsize >> 8) & 0xff, 8, &bpos);
		bits_write(&bp, bsize&0xff, 8, &bpos);
	}
	while (1) {
		if (inSize <= count * 4) nodata = 1;
		if (nodata) break;
		one[0] = inData[count * 4];
		one[1] = inData[count * 4 + 1];
		one[2] = inData[count * 4 + 2];
		one[3] = inData[count * 4 + 3];

#if BYTE_ORDER == BIG_ENDIAN
		bits_write(&bp, one[0], 8, &bpos);
		bits_write(&bp, one[1], 8, &bpos);
		bits_write(&bp, one[2], 8, &bpos);
		bits_write(&bp, one[3], 8, &bpos);
#else
		bits_write(&bp, one[1], 8, &bpos);
		bits_write(&bp, one[0], 8, &bpos);
		bits_write(&bp, one[3], 8, &bpos);
		bits_write(&bp, one[2], 8, &bpos);
#endif

		if (++count == bsize) break;
	}
	if (!count) return false; // when no data at all, it should stop playing
	/* when readable size is less than bsize, fill 0 at the bottom */
	for(i = 0; i < (bsize - count) * 4; i++) {
		bits_write(&bp, 0, 8, &bpos);
	}
	*size = (int)(bp - buffer);
	if (bpos) *size += 1;
	return true;
}

static void
raopcl_stream_connect(G_GNUC_UNUSED struct raop_data *rd)
{
}


static bool
raopcl_connect(struct raop_data *rd, GError **error_r)
{
	unsigned char buf[4 + 8 + 16];
	char sid[16];
	char sci[24];
	char act_r[17];
	char *sac=NULL, *key = NULL, *iv = NULL;
	char sdp[1024];
	int rval = false;
	struct key_data *setup_kd = NULL;
	char *aj, *token, *pc;
	const char delimiters[] = ";";
	unsigned char rsakey[512];
	struct timeval current_time;
	unsigned int sessionNum;
	int i;


	gettimeofday(&current_time,NULL);
	sessionNum = current_time.tv_sec + 2082844804;

	RAND_bytes(buf, sizeof(buf));
	sprintf(act_r, "%u", (unsigned int) g_random_int());
	sprintf(sid, "%u", sessionNum);
	sprintf(sci, "%08x%08x", *((int *)(buf + 4)), *((int *)(buf + 8)));
	sac = g_base64_encode(buf + 12, 16);
	rd->rtspcl = rtspcl_open();
	rtspcl_set_useragent(rd->rtspcl, "iTunes/8.1.1 (Macintosh; U; PPC Mac OS X 10.4)");
	rtspcl_add_exthds(rd->rtspcl, "Client-Instance", sci);
	rtspcl_add_exthds(rd->rtspcl, "DACP-ID", sci);
	rtspcl_add_exthds(rd->rtspcl, "Active-Remote", act_r);
	if (!rtspcl_connect(rd->rtspcl, rd->addr, rd->rtsp_port, sid, error_r))
		goto erexit;

	i = rsa_encrypt(raop_session->encrypt.key, 16, rsakey);
	key = g_base64_encode(rsakey, i);
	remove_char_from_string(key, '=');
	iv = g_base64_encode(raop_session->encrypt.iv, 16);
	remove_char_from_string(iv, '=');
	sprintf(sdp,
		"v=0\r\n"
		"o=iTunes %s 0 IN IP4 %s\r\n"
		"s=iTunes\r\n"
		"c=IN IP4 %s\r\n"
		"t=0 0\r\n"
		"m=audio 0 RTP/AVP 96\r\n"
		"a=rtpmap:96 AppleLossless\r\n"
		"a=fmtp:96 %d  0 16 40 10 14 2 255 0 0 44100\r\n"
		"a=rsaaeskey:%s\r\n"
		"a=aesiv:%s\r\n",
		sid, rtspcl_local_ip(rd->rtspcl), rd->addr, NUMSAMPLES, key, iv);
	remove_char_from_string(sac, '=');
	// rtspcl_add_exthds(rd->rtspcl, "Apple-Challenge", sac);
	if (!rtspcl_announce_sdp(rd->rtspcl, sdp, error_r))
		goto erexit;
	//	if (!rtspcl_mark_del_exthds(rd->rtspcl, "Apple-Challenge")) goto erexit;
	if (!rtspcl_setup(rd->rtspcl, &setup_kd,
			  raop_session->ctrl.port, raop_session->ntp.port,
			  error_r))
		goto erexit;
	if (!(aj = kd_lookup(setup_kd,"Audio-Jack-Status"))) {
		g_set_error_literal(error_r, raop_output_quark(), 0,
				    "Audio-Jack-Status is missing");
		goto erexit;
	}

	token = strtok(aj, delimiters);
	while (token) {
		if ((pc = strstr(token,"="))) {
			*pc = 0;
			if (!strcmp(token,"type") && !strcmp(pc+1,"digital")) {
				//				rd->ajtype = JACK_TYPE_DIGITAL;
			}
		} else {
			if (!strcmp(token,"connected")) {
				//				rd->ajstatus = JACK_STATUS_CONNECTED;
			}
		}
		token = strtok(NULL, delimiters);
	}

	if (!get_sockaddr_by_host(rd->addr, rd->rtspcl->control_port,
				  &rd->ctrl_addr, error_r))
		goto erexit;

	if (!get_sockaddr_by_host(rd->addr, rd->rtspcl->server_port,
				  &rd->data_addr, error_r))
		goto erexit;

	if (!rtspcl_record(rd->rtspcl,
			   raop_session->play_state.seq_num,
			   raop_session->play_state.rtptime,
			   error_r))
		goto erexit;

	raopcl_stream_connect(rd);

	rval = true;

 erexit:
	g_free(sac);
	g_free(key);
	g_free(iv);
	free_kd(setup_kd);
	return rval;
}

static void
raopcl_close(struct raop_data *rd)
{
	if (rd->rtspcl)
		rtspcl_close(rd->rtspcl);
	rd->rtspcl = NULL;
	g_free(rd);
}

static int
difference (struct timeval *t1, struct timeval *t2)
{
	int ret = 150000000;
	if (t1->tv_sec - t2->tv_sec < 150) {
		ret = (t1->tv_sec - t2->tv_sec) * 1000000;
		ret += t1->tv_usec - t2->tv_usec;
	}
	return ret;
}

/*
 * With airtunes version 2, we don't get responses back when we send audio
 * data.  The only requests we get from the airtunes device are timing
 * requests.
 */
static bool
send_audio_data(int fd, GError **error_r)
{
	int i = 0;
	struct timeval current_time, rtp_time;
	struct raop_data *rd = raop_session->raop_list;

	get_time_for_rtp(&raop_session->play_state, &rtp_time);
	gettimeofday(&current_time, NULL);
	int diff = difference(&current_time, &rtp_time);
	g_usleep(-diff);

	gettimeofday(&raop_session->play_state.last_send, NULL);
	while (rd) {
		if (rd->started) {
			raop_session->data[1] = 0x60;
		} else {
			rd->started = true;
			raop_session->data[1] = 0xe0;
		}
		i = sendto(fd, raop_session->data + raop_session->wblk_wsize,
			   raop_session->wblk_remsize, 0, (struct sockaddr *) &rd->data_addr,
			   sizeof(rd->data_addr));
		if (i < 0) {
			g_set_error(error_r, raop_output_quark(), errno,
				    "write error: %s",
				    g_strerror(errno));
			return false;
		}
		if (i == 0) {
			g_set_error_literal(error_r, raop_output_quark(), 0,
					    "disconnected on the other end");
			return false;
		}
		rd = rd->next;
	}
	raop_session->wblk_wsize += i;
	raop_session->wblk_remsize -= i;

	return true;
}

static void *
raop_output_init(G_GNUC_UNUSED const struct audio_format *audio_format,
		 G_GNUC_UNUSED const struct config_param *param,
		 GError **error_r)
{
	const char *host = config_get_block_string(param, "host", NULL);
	if (host == NULL) {
		g_set_error_literal(error_r, raop_output_quark(), 0,
				    "missing option 'host'");
		return NULL;
	}

	struct raop_data *rd;

	rd = new_raop_data(error_r);
	if (rd == NULL)
		return NULL;

	rd->addr = host;
	rd->rtsp_port = config_get_block_unsigned(param, "port", 5000);
	rd->volume = config_get_block_unsigned(param, "volume", 75);
	return rd;
}

static bool
raop_set_volume_local(struct raop_data *rd, int volume, GError **error_r)
{
	char vol_str[128];
	sprintf(vol_str, "volume: %d.000000\r\n", volume);
	return rtspcl_set_parameter(rd->rtspcl, vol_str, error_r);
}


static void
raop_output_finish(void *data)
{
	struct raop_data *rd = data;
	raopcl_close(rd);
	g_mutex_free(rd->control_mutex);
	g_free(rd);
}

#define RAOP_VOLUME_MIN -30
#define RAOP_VOLUME_MAX 0

int
raop_get_volume(struct raop_data *rd)
{
	return rd->volume;
}

bool
raop_set_volume(struct raop_data *rd, unsigned volume, GError **error_r)
{
	int raop_volume;
	bool rval;

	//set parameter volume
	if (volume == 0) {
		raop_volume = -144;
	} else {
		raop_volume = RAOP_VOLUME_MIN +
			(RAOP_VOLUME_MAX - RAOP_VOLUME_MIN) * volume / 100;
	}
	g_mutex_lock(rd->control_mutex);
	rval = raop_set_volume_local(rd, raop_volume, error_r);
	if (rval) rd->volume = volume;
	g_mutex_unlock(rd->control_mutex);

	return rval;
}

static void
raop_output_cancel(void *data)
{
	//flush
	struct key_data kd;
	struct raop_data *rd = (struct raop_data *) data;
	int flush_diff = 1;

	rd->started = 0;
	if (rd->is_master) {
		raop_session->play_state.playing = false;
	}
	if (rd->paused) {
		return;
	}

	g_mutex_lock(rd->control_mutex);
	static char rtp_key[] = "RTP-Info";
	kd.key = rtp_key;
	char buf[128];
	sprintf(buf, "seq=%d; rtptime=%d", raop_session->play_state.seq_num + flush_diff, raop_session->play_state.rtptime + NUMSAMPLES * flush_diff);
	kd.data = buf;
	kd.next = NULL;
	exec_request(rd->rtspcl, "FLUSH", NULL, NULL, 1,
		     &kd, &(rd->rtspcl->kd), NULL);
	g_mutex_unlock(rd->control_mutex);
}

static bool
raop_output_pause(void *data)
{
	struct raop_data *rd = (struct raop_data *) data;

	rd->paused = true;
	return true;
}

/**
 * Remove the output from the session's list.  Caller must not lock
 * the list_mutex.
 */
static void
raop_output_remove(struct raop_data *rd)
{
	struct raop_data *iter = raop_session->raop_list;
	struct raop_data *prev = NULL;

	g_mutex_lock(raop_session->list_mutex);
	while (iter) {
		if (iter == rd) {
			if (prev != NULL) {
				prev->next = rd->next;
			} else {
				raop_session->raop_list = rd->next;
				if (raop_session->raop_list == NULL) {
					// TODO clean up everything else
					raop_session->play_state.playing = false;
					close(raop_session->data_fd);
					ntp_server_close(&raop_session->ntp);
					close(raop_session->ctrl.fd);
				}
			}
			if (rd->is_master && raop_session->raop_list) {
				raop_session->raop_list->is_master = true;
			}
			rd->next = NULL;
			rd->is_master = false;
			break;
		}
		prev = iter;
		iter = iter->next;
	}
	g_mutex_unlock(raop_session->list_mutex);
}

static void
raop_output_close(void *data)
{
	//teardown
	struct raop_data *rd = data;

	raop_output_remove(rd);

	g_mutex_lock(rd->control_mutex);
	exec_request(rd->rtspcl, "TEARDOWN", NULL, NULL, 0,
		     NULL, &rd->rtspcl->kd, NULL);
	g_mutex_unlock(rd->control_mutex);

	rd->started = 0;
}


static bool
raop_output_open(void *data, struct audio_format *audio_format, GError **error_r)
{
	//setup, etc.
	struct raop_data *rd = data;

	g_mutex_lock(raop_session->list_mutex);
	if (raop_session->raop_list == NULL) {
		// first raop, need to initialize session data
		unsigned short myport = 0;
		raop_session->raop_list = rd;
		rd->is_master = true;

		raop_session->data_fd = open_udp_socket(NULL, &myport,
							error_r);
		if (raop_session->data_fd < 0)
			return false;

		if (!ntp_server_open(&raop_session->ntp, error_r))
			return false;

		raop_session->ctrl.fd =
			open_udp_socket(NULL, &raop_session->ctrl.port,
					error_r);
		if (raop_session->ctrl.fd < 0) {
			ntp_server_close(&raop_session->ntp);
			raop_session->ctrl.fd = -1;
			g_mutex_unlock(raop_session->list_mutex);
			return false;
		}
	}
	g_mutex_unlock(raop_session->list_mutex);

	audio_format->format = SAMPLE_FORMAT_S16;
	if (!raopcl_connect(rd, error_r)) {
		raop_output_remove(rd);
		return false;
	}

	if (!raop_set_volume(rd, rd->volume, error_r)) {
		raop_output_remove(rd);
		return false;
	}

	g_mutex_lock(raop_session->list_mutex);
	if (!rd->is_master) {
		rd->next = raop_session->raop_list;
		raop_session->raop_list = rd;
	}
	g_mutex_unlock(raop_session->list_mutex);
	return true;
}

static size_t
raop_output_play(void *data, const void *chunk, size_t size,
		 GError **error_r)
{
	//raopcl_send_sample
	struct raop_data *rd = data;
	size_t rval = 0, orig_size = size;

	rd->paused = false;
	if (!rd->is_master) {
		// only process data for the master raop
		return size;
	}

	g_mutex_lock(raop_session->data_mutex);

	if (raop_session->play_state.rtptime <= NUMSAMPLES) {
		// looped over, need new reference point to calculate correct times
		raop_session->play_state.playing = false;
	}

	while (raop_session->bufferSize + size >= RAOP_BUFFER_SIZE) {
		// ntp header
		unsigned char header[] = {
			0x80, 0x60, 0x00, 0x00,
			// rtptime
			0x00, 0x00, 0x00, 0x00,
			// device
			0x7e, 0xad, 0xd2, 0xd3,
		};


		int count = 0;
		int copyBytes = RAOP_BUFFER_SIZE - raop_session->bufferSize;

		if (!raop_session->play_state.playing ||
		    raop_session->play_state.seq_num % (44100 / NUMSAMPLES + 1) == 0) {
			struct raop_data *iter;
			g_mutex_lock(raop_session->list_mutex);
			if (!raop_session->play_state.playing) {
				gettimeofday(&raop_session->play_state.start_time,NULL);
			}
			iter = raop_session->raop_list;
			while (iter) {
				if (!send_control_command(&raop_session->ctrl, iter,
							  &raop_session->play_state,
							  error_r))
					goto erexit;

				iter = iter->next;
			}
			g_mutex_unlock(raop_session->list_mutex);
		}

		fill_int(header + 8, raop_session->play_state.sync_src);

		memcpy(raop_session->buffer + raop_session->bufferSize, chunk, copyBytes);
		raop_session->bufferSize += copyBytes;
		chunk = ((const char *)chunk) + copyBytes;
		size -= copyBytes;

		if (!wrap_pcm(raop_session->data + RAOP_HEADER_SIZE, NUMSAMPLES, &count, raop_session->buffer, RAOP_BUFFER_SIZE)) {
			g_warning("unable to encode %d bytes properly\n", RAOP_BUFFER_SIZE);
		}

		memcpy(raop_session->data, header, RAOP_HEADER_SIZE);
		raop_session->data[2] = raop_session->play_state.seq_num >> 8;
		raop_session->data[3] = raop_session->play_state.seq_num & 0xff;
		raop_session->play_state.seq_num ++;

		fill_int(raop_session->data + 4, raop_session->play_state.rtptime);
		raop_session->play_state.rtptime += NUMSAMPLES;

		raop_encrypt(&raop_session->encrypt, raop_session->data + RAOP_HEADER_SIZE, count);
		raop_session->wblk_remsize = count + RAOP_HEADER_SIZE;
		raop_session->wblk_wsize = 0;

		if (!send_audio_data(raop_session->data_fd, error_r))
			goto erexit;

		raop_session->bufferSize = 0;
	}
	if (size > 0) {
		memcpy(raop_session->buffer + raop_session->bufferSize, chunk, size);
		raop_session->bufferSize += size;
	}
	rval = orig_size;
 erexit:
	g_mutex_unlock(raop_session->data_mutex);
	return rval;
}

const struct audio_output_plugin raopPlugin = {
	.name = "raop",
	.init = raop_output_init,
	.finish = raop_output_finish,
	.open = raop_output_open,
	.play = raop_output_play,
	.cancel = raop_output_cancel,
	.pause = raop_output_pause,
	.close = raop_output_close,
	.mixer_plugin = &raop_mixer_plugin,
};
