#include "CdioParanoiaCDID.hxx"
#include "lib/cdio/Paranoia.hxx"

#include <vector>

extern "C" {
#include <libavutil/sha.h>
#include <libavutil/base64.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* close() */
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#if defined(__linux__)
#include <linux/cdrom.h>
#define cdte_track_address      cdte_addr.lba
#define DEVICE_NAME             "/dev/cdrom"

#elif defined(__GNU__)
/* According to Samuel Thibault <sthibault@debian.org>, cd-discid needs this
 * to compile on Debian GNU/Hurd (i386) */
#include <sys/cdrom.h>
#define cdte_track_address      cdte_addr.lba
#define DEVICE_NAME             "/dev/cd0"

#elif defined(sun) && defined(unix) && defined(__SVR4)
#include <sys/cdio.h>
#define CD_MSF_OFFSET   150
#define CD_FRAMES       75
/* According to David Schweikert <dws@ee.ethz.ch>, cd-discid needs this
 * to compile on Solaris */
#define cdte_track_address      cdte_addr.lba
#define DEVICE_NAME             "/dev/vol/aliases/cdrom0"

/* __FreeBSD_kernel__ is needed for properly compiling on Debian GNU/kFreeBSD
   Look at http://glibc-bsd.alioth.debian.org/porting/PORTING for more info */
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
#include <sys/cdio.h>
#define CDROM_LBA               CD_LBA_FORMAT    /* first frame is 0 */
#define CD_MSF_OFFSET           150              /* MSF offset of first frame */
#define CD_FRAMES               75               /* per second */
#define CDROM_LEADOUT           0xAA             /* leadout track */
#define CDROMREADTOCHDR         CDIOREADTOCHEADER
#define CDROMREADTOCENTRY       CDIOREADTOCENTRY
#define cdrom_tochdr            ioc_toc_header
#define cdth_trk0               starting_track
#define cdth_trk1               ending_track
#define cdrom_tocentry          ioc_read_toc_single_entry
#define cdte_track              track
#define cdte_format             address_format
#define cdte_track_address      entry.addr.lba
#define DEVICE_NAME             "/dev/cdrom"

#elif defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/cdio.h>
#define CDROM_LBA               CD_LBA_FORMAT    /* first frame is 0 */
#define CD_MSF_OFFSET           150              /* MSF offset of first frame */
#define CD_FRAMES               75               /* per second */
#define CDROM_LEADOUT           0xAA             /* leadout track */
#define CDROMREADTOCHDR         CDIOREADTOCHEADER
#define cdrom_tochdr            ioc_toc_header
#define cdth_trk0               starting_track
#define cdth_trk1               ending_track
#define cdrom_tocentry          cd_toc_entry
#define cdte_track              track
#define cdte_track_address      addr.lba
#define DEVICE_NAME             "/dev/cd0a"

#elif defined(__APPLE__)
#include <sys/types.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#define CD_FRAMES               75               /* per second */
#define CD_MSF_OFFSET           150              /* MSF offset of first frame */
#define cdrom_tochdr            CDDiscInfo
#define cdth_trk0               numberOfFirstTrack
/* NOTE: Judging by the name here, we might have to do this:
 * hdr.lastTrackNumberInLastSessionMSB << 8 *
 * sizeof(hdr.lastTrackNumberInLastSessionLSB)
 * | hdr.lastTrackNumberInLastSessionLSB; */
#define cdth_trk1               lastTrackNumberInLastSessionLSB
#define cdrom_tocentry          CDTrackInfo
#define cdte_track_address      trackStartAddress
#define DEVICE_NAME             "/dev/rdisk1"

#elif defined(__sgi)
#include <dmedia/cdaudio.h>
#define CD_FRAMES               75               /* per second */
#define CD_MSF_OFFSET           150              /* MSF offset of first frame */
#define cdrom_tochdr            CDSTATUS
#define cdth_trk0               first
#define cdth_trk1               last
#define close                   CDclose
struct cdrom_tocentry
{
	int cdte_track_address;
};
#define DEVICE_NAME             "/dev/scsi/sc0d4l0"
#else
#error "Your OS isn't supported yet."
#endif  /* os selection */
}

static char*
makeMusicBrainzIdWith(int firstTrack, int lastTrack, int leadIn, std::vector<int> frameOffsets)
{
	struct AVSHA* sha1 = av_sha_alloc();
	const int numBitsSha1 = 160;
	const int digestSize = numBitsSha1 / 8;
	uint8_t digest[digestSize];
	const int tempSize = 32;
	char temp[tempSize];

	av_sha_init(sha1, numBitsSha1);

	snprintf(temp, tempSize, "%02X", firstTrack);
	av_sha_update(sha1, (const uint8_t*)temp, strlen(temp));

	snprintf(temp, tempSize, "%02X", lastTrack);
	av_sha_update(sha1, (const uint8_t*)temp, strlen(temp));

	for (size_t i = 0; i < 100; ++i)
	{
		int offset = 0;

		if (i < frameOffsets.size())
			offset = frameOffsets[i] + leadIn;
		snprintf(temp, tempSize, "%08X", offset);
		av_sha_update(sha1, (const uint8_t*)temp, strlen(temp));
	}
	av_sha_final(sha1, digest);
	free(sha1);

	const int outputSize = 128;
	char *output = new char[outputSize];

	av_base64_encode(output, outputSize, digest, digestSize);

	for (size_t i = 0; i < strlen(output); ++i)
	{
		switch (output[i])
		{
		case '=':
			output[i] = '-';
			break;
		case '/':
			output[i] = '_';
			break;
		case '+':
			output[i] = '.';
			break;
		default:
			break;
		}
	}

	return output;
}

// getCurrentCDId is ported from https://github.com/taem/cd-discid

std::string
CDIODiscID::getCurrentCDId (std::string device)
{
	int len;
	int i;
	long int cksum = 0;
	//int musicbrainz = 0;
	unsigned char last = 1;
	const char *devicename = device.c_str();
	struct cdrom_tocentry *TocEntry;
#ifndef __sgi
	int drive;
	struct cdrom_tochdr hdr;
#else
	CDPLAYER *drive;
	CDTRACKINFO info;
	cdrom_tochdr hdr;
#endif
	//char *command = argv[0];

#if defined(__OpenBSD__) || defined(__NetBSD__)
	struct ioc_read_toc_entry t;
#elif defined(__APPLE__)
	dk_cd_read_disc_info_t discInfoParams;
#endif

#if defined(__sgi)
	drive = CDopen(devicename, "r");
	if (drive == 0) {
		return {};
	}
#else
	drive = open(devicename, O_RDONLY | O_NONBLOCK);
	if (drive < 0) {
		return {};
	}
#endif

#if defined(__APPLE__)
	memset(&discInfoParams, 0, sizeof(discInfoParams));
	discInfoParams.buffer = &hdr;
	discInfoParams.bufferLength = sizeof(hdr);
	if (ioctl(drive, DKIOCCDREADDISCINFO, &discInfoParams) < 0
			|| discInfoParams.bufferLength != sizeof(hdr)) {
		return {};
	}
#elif defined(__sgi)
	if (CDgetstatus(drive, &hdr) == 0) {
		return {}
	}
#else
	if (ioctl(drive, CDROMREADTOCHDR, &hdr) < 0) {
		return {};
	}
#endif

	last = hdr.cdth_trk1;

	len = (last + 1) * sizeof(struct cdrom_tocentry);

	TocEntry = (cdrom_tocentry*)malloc(len);
	if (!TocEntry) {
		return {};
	}

#if defined(__OpenBSD__)
	t.starting_track = 0;
#elif defined(__NetBSD__)
	t.starting_track = 1;
#endif

#if defined(__OpenBSD__) || defined(__NetBSD__)
	t.address_format = CDROM_LBA;
	t.data_len = len;
	t.data = TocEntry;
	memset(TocEntry, 0, len);

	if (ioctl(drive, CDIOREADTOCENTRYS, (char*)&t) < 0) {
		return {};
	}
#elif defined(__APPLE__)
	dk_cd_read_track_info_t trackInfoParams;
	memset(&trackInfoParams, 0, sizeof(trackInfoParams));
	trackInfoParams.addressType = kCDTrackInfoAddressTypeTrackNumber;
	trackInfoParams.bufferLength = sizeof(*TocEntry);

	for (i = 0; i < last; i++) {
		trackInfoParams.address = i + 1;
		trackInfoParams.buffer = &TocEntry[i];

		if (ioctl(drive, DKIOCCDREADTRACKINFO, &trackInfoParams) < 0) {
			return {};
		}
	}

	/* MacOS X on G5-based systems does not report valid info for
	 * TocEntry[last-1].lastRecordedAddress + 1, so we compute the start
	 * of leadout from the start+length of the last track instead
	 */
	TocEntry[last].cdte_track_address = htonl(ntohl(TocEntry[last-1].trackSize) + ntohl(TocEntry[last-1].trackStartAddress));
#elif defined(__sgi)
	for (i = 0; i < last; i++) {
		if (CDgettrackinfo(drive, i + 1, &info) == 0) {
			return {};
		}
		TocEntry[i].cdte_track_address = info.start_min*60*CD_FRAMES + info.start_sec*CD_FRAMES + info.start_frame;
	}
	TocEntry[last].cdte_track_address = TocEntry[last - 1].cdte_track_address + info.total_min*60*CD_FRAMES + info.total_sec*CD_FRAMES + info.total_frame;
#else   /* FreeBSD, Linux, Solaris */
	for (i = 0; i < last; i++) {
		/* tracks start with 1, but I must start with 0 on OpenBSD */
		TocEntry[i].cdte_track = i + 1;
		TocEntry[i].cdte_format = CDROM_LBA;
		if (ioctl(drive, CDROMREADTOCENTRY, &TocEntry[i]) < 0) {
			return {};
		}
	}

	TocEntry[last].cdte_track = CDROM_LEADOUT;
	TocEntry[last].cdte_format = CDROM_LBA;
	if (ioctl(drive, CDROMREADTOCENTRY, &TocEntry[i]) < 0) {
		return {};
	}
#endif

	/* release file handle */
	close(drive);

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__APPLE__)
	TocEntry[i].cdte_track_address = ntohl(TocEntry[i].cdte_track_address);
#endif

	for (i = 0; i < last; i++) {
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__APPLE__)
		TocEntry[i].cdte_track_address = ntohl(TocEntry[i].cdte_track_address);
#endif
		cksum += cddb_sum((TocEntry[i].cdte_track_address + CD_MSF_OFFSET) / CD_FRAMES);
	}

	/*
	 *totaltime = ((TocEntry[last].cdte_track_address + CD_MSF_OFFSET) / CD_FRAMES) -
	 *    ((TocEntry[0].cdte_track_address + CD_MSF_OFFSET) / CD_FRAMES);
	 */

	/*
	 *[> print discid <]
	 *if (!musicbrainz)
	 *    printf("%08lx ", (cksum % 0xff) << 24 | totaltime << 8 | last);
	 */

	/* print number of tracks */
	//printf("%d", last);
	int numTracks = last;
	int firstTrackNumber = 1;

	std::vector<int> frameOffsets;

	if (last > 0)
		frameOffsets.push_back(TocEntry[last].cdte_track_address);

	/* print frame offsets of all tracks */
	for (i = 0; i < last; i++)
	{
		//printf(" %d", TocEntry[i].cdte_track_address + CD_MSF_OFFSET);
		frameOffsets.push_back(TocEntry[i].cdte_track_address);
	}

	free(TocEntry);

	int leadIn = CDIO_PREGAP_SECTORS;
	int lastTrack = numTracks + firstTrackNumber - 1;

	auto musicId = makeMusicBrainzIdWith(firstTrackNumber, lastTrack, leadIn, frameOffsets);

	std::string musicIdString(musicId);

	delete [] musicId;

	return musicIdString;
}

int
CDIODiscID::cddb_sum(int n)
{
	/* a number like 2344 becomes 2+3+4+4 (13) */
	int ret = 0;

	while (n > 0) {
		ret = ret + (n % 10);
		n = n / 10;
	}

	return ret;
}
