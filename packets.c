/*
 * gigargoyle
 *
 * a nighttime composition in opensource
 *
 * this is part of the 2010 binkenlights installation
 * in Giesing, Munich, Germany which is called
 *
 *   a.c.a.b. - all colors are beautiful
 *
 * the installation is run by the Chaos Computer Club Munich
 * as part of the puerto giesing
 * 
 *
 * license:
 *          GPL v2, see the file LICENSE
 * authors:
 *          Matthias Wenzel - aka - mazzoo
 *
 */

#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "packets.h"
#include "fifo.h"
#include "gigargoyle.h"
#include "command_line_arguments.h"

/* Contains parsed command line arguments */
extern struct arguments arguments;

void write_all(int fd, char *str, int cnt)
{
	int res;
	int towrite = cnt;

	while (towrite > 0 && (res = write(fd, str, cnt)) != towrite) {
		if (res < 0 && errno == EINTR)
			continue;
		if (res < 0) {
			/* FIXME: Error, handle it! */
			break;
		}

		towrite -= res;
		str += res;
	}
}

int check_packet(pkt_t * p, uint32_t plen)
{
	if (p->pkt_len < 8)
	{
		LOG("QM: short packet tells me its %d bytes long\n", p->pkt_len);
		return -2;
	}

	if (plen < 8)
	{
		LOG("PKTS: WARNING: got very short packet (len %d)\n", plen);
		return -1;
	}

	if ((p->hdr & PKT_MASK_VERSION) != VERSION << VERSION_SHIFT)
	{
		LOG("PKTS: WARNING: dropping pkt with invalid version, hdr %x\n", p->hdr);
		return -2; /* drop wrong version packets */
	}

	if (plen < p->pkt_len )
	{
#if 0 /* this is fine -1 is the marker for incomplete packets */
		LOG("PKTS: WARNING: got short packet (%d < %d)\n",
		    plen, p->pkt_len
		   );
#endif
		return -1;
	}

	if (p->pkt_len > FIFO_WIDTH)
	{
		LOG("PKTS: WARNING: got long packet (len %d)\n", plen);
		return -3;
	}

	return 0;
}

/* handle packets after being received by socket */
void early_handle_packet(pkt_t * p) {
	switch(p->hdr & PKT_MASK_TYPE)
	{
		case PKT_TYPE_SET_SCREEN_BLK:
		case PKT_TYPE_SET_SCREEN_WHT:
		case PKT_TYPE_SET_SCREEN_RND_BW:
		case PKT_TYPE_SET_SCREEN_RND_COL:
		case PKT_TYPE_SET_FRAME_RATE:
		case PKT_TYPE_SET_FADE_RATE:
		case PKT_TYPE_SET_DURATION:
		case PKT_TYPE_SET_PIXEL:
		case PKT_TYPE_SET_SCREEN:
		case PKT_TYPE_FLIP_DBL_BUF:
		case PKT_TYPE_TEXT:
		case PKT_TYPE_SET_FONT:
			wr_fifo(p);
			break;
		/* out-of-band immediate commands follow */
		case PKT_TYPE_FLUSH_FIFO:
			flush_fifo();
			break;
		case PKT_TYPE_SHUTDOWN:
			gigargoyle_shutdown(); /* FIXME only from QM, not from IS */
			break;
		default:
			break; /* drop unsupported packages */
	}
}

void set_pixel_xy_rgb8(
                  uint32_t hdr,
                  uint8_t r,
                  uint8_t g,
                  uint8_t b,
                  uint16_t x,
                  uint16_t y
                 ){
	static uint64_t last_timestamp[ACAB_Y] =
	                {0, 0, 0, 0, 0, 0}; /* FIXME flexible row/bus mapping */

	uint64_t       timestamp;

	shadow_screen[y][x][0] = r;
	shadow_screen[y][x][1] = g;
	shadow_screen[y][x][2] = b;

	y = ACAB_Y - y - 1;

        /* Mapping hack */
        //if(y % 2 == 1)
        //  x = ACAB_X - 1 - x;

	uint8_t bus_buf[24];
	bus_buf[0] = 0x5c;
	bus_buf[1] = 0x30;
	bus_buf[2] = 0x1F - x + 0x10*y;
	bus_buf[3] = 'C';
	bus_buf[4] = r;
	bus_buf[5] = g;
	bus_buf[6] = b;
	bus_buf[7] = 0x5c;
	bus_buf[8] = 0x31;

	int bus_buf_len = 9;

	// escaping
	int i, j;
	for(i=1; i<bus_buf_len-2; i++) {
	    if(bus_buf[i] == '\\') {
		bus_buf_len++;
		for(j=bus_buf_len-1; j > i+1; j--)
		    bus_buf[j] = bus_buf[j-1];
		i++;
		bus_buf[i] = '\\';
	    }
	}

	//printf("%i/%i %x\n", y, x, bus_buf[2]);

	/* FIXME: doesn't work...
	if (hdr & PKT_MASK_DBL_BUF)
		bus_buf[3] = 'P';
	*/

	timestamp = gettimeofday64();

	if (last_timestamp[y] + MIN_GAP_BUS_TRANSFERS > timestamp)
	{
		usleep(last_timestamp[y] + MIN_GAP_BUS_TRANSFERS - timestamp);
		timestamp = gettimeofday64();
	}

	int ret = write(ggg->uart[y], bus_buf, bus_buf_len);

	if (ret != 9)
		LOG("PKTS: WARNING: write(bus %d) = %d != 9\n", y, ret);
	last_timestamp[y] = timestamp;
}

void set_pixel_xy_rgb16(
                  uint32_t hdr,
                  uint16_t r,
                  uint16_t g,
                  uint16_t b,
                  uint16_t x,
                  uint16_t y
                 ){
	/* FIXME */
	set_pixel_xy_rgb8(hdr, r, g, b, x, y);
}

void set_screen_blk(void)
{
	int ix, iy;
	for (iy=0; iy < ACAB_Y; iy++)
	{
		for (ix=0; ix < ACAB_X; ix++)
		{
			set_pixel_xy_rgb8(0, 0, 0, 0, ix, iy); /* FIXME 0 */
		}
	}
}

void set_screen_rgb8(uint32_t hdr, uint8_t s[ACAB_Y][ACAB_X][3])
{
	int ix, iy;

	for (ix=0; ix < ACAB_X; ix++)
	{
		for (iy=0; iy < ACAB_Y; iy++)
		{
			/* LOG("%x %x %x, ", s[ix][iy][0], s[ix][iy][1], s[ix][iy][2]); */
			set_pixel_xy_rgb8(
			                hdr,
			                s[iy][ix][0],
			                s[iy][ix][1],
			                s[iy][ix][2],
					ix, iy
				    );
		}
	}

	if (hdr & PKT_MASK_REQ_ACK && ggg->source == SOURCE_QM &&
			ggg->qm->state == NET_CONNECTED)
	{
		//LOG("PKTS: ACK\n");
		write_all(ggg->qm->sock, ACK_AB_KLINGON, strlen(ACK_AB_KLINGON));
	}
}

void set_screen_rgb16(uint32_t hdr, uint16_t s[ACAB_Y][ACAB_X][3])
{
	int ix, iy;
	for (ix=0; ix < ACAB_X; ix++)
	{
		for (iy=0; iy < ACAB_Y; iy++)
		{
			set_pixel_xy_rgb16(
			                hdr,
			                s[iy][ix][0],
			                s[iy][ix][1],
			                s[iy][ix][2],
					ix, iy
				    );
		}
	}

	if (hdr & PKT_MASK_REQ_ACK && ggg->source == SOURCE_QM &&
                    ggg->qm->state == NET_CONNECTED)
		write_all(ggg->qm->sock, ACK_AB_KLINGON, strlen(ACK_AB_KLINGON));
}

void set_screen_rnd_bw(void)
{
	int ix, iy;
	for (iy=0; iy < ACAB_Y; iy++)
	{
		for (ix=0; ix < ACAB_X; ix++)
		{
			if (random()&1)
			{
				tmp_screen8[iy][ix][0] = 0xff;
				tmp_screen8[iy][ix][1] = 0xff;
				tmp_screen8[iy][ix][2] = 0xff;
			}else{
				tmp_screen8[iy][ix][0] = 0x00;
				tmp_screen8[iy][ix][1] = 0x00;
				tmp_screen8[iy][ix][2] = 0x00;
			}
		}
	}
	set_screen_rgb8(0, tmp_screen8); /* FIXME 0 */
}

void set_screen_rnd_col(void)
{
	int ix, iy;
	for (iy=0; iy < ACAB_Y; iy++)
	{
		for (ix=0; ix < ACAB_X; ix++)
		{
			tmp_screen8[iy][ix][0] = random();
			tmp_screen8[iy][ix][1] = random();
			tmp_screen8[iy][ix][2] = random();
		}
	}
	set_screen_rgb8(0, tmp_screen8); /* FIXME 0 */
}

void flip_double_buffer_on_bus(int b)
{
	return;
	uint8_t bus_buf[9]; //FIXME
	bus_buf[0] = 0x5c;
	bus_buf[1] = 0x30;
	bus_buf[2] = 0x00;
	bus_buf[3] = 'U';
	bus_buf[7] = 0x5c;
	bus_buf[8] = 0x31;
	int ret;

	ret = write(ggg->uart[b], bus_buf, 9);
	if (ret != 9)
		LOG("PKTS: WARNING: flip_double_buffer_on_bus() write(bus %d) = %d != 9\n", b, ret);
}

void flip_double_buffer(void)
{
	int i;
	for (i=0; i<ACAB_Y; i++)
		flip_double_buffer_on_bus(i);
}

void send_reset_on_bus(int b)
{
	uint8_t bus_buf[6]; //FIXME
	bus_buf[0] = 0x5c;
	bus_buf[1] = 0x30;
	bus_buf[2] = 0x00;
	bus_buf[3] = 'r';
	bus_buf[4] = 0x5c;
	bus_buf[5] = 0x31;
	int ret;

	ret = write(ggg->uart[b], bus_buf, 6);
	if (ret != 6)
		LOG("PKTS: WARNING: reset_on_bus() write(bus %d) = %d != 6\n", b, ret);
}

void send_reset(void)
{
	int i;
	//for (i=0; i<ACAB_Y; i++)
		//send_reset_on_bus(i);
}

void next_frame(void)
{
	pkt_t * p;
	p = rd_fifo();

	if (p == NULL)
	{
		LOG("PKTS: next_frame() ran into an empty fifo: network timeout: %d/%d\n", MISSING_PKTS_TO_LOCAL - ggg->fifo->running_empty_on_network, MISSING_PKTS_TO_LOCAL);
		return;
	}

	handle_packet(p);
}


void handle_packet(pkt_t * p) {
	switch(p->hdr & PKT_MASK_TYPE)
	{
		case PKT_TYPE_SET_FADE_RATE:
		case PKT_TYPE_SET_PIXEL:
		case PKT_TYPE_FLIP_DBL_BUF:
		case PKT_TYPE_TEXT:
		case PKT_TYPE_SET_FONT:
		case PKT_TYPE_SET_SCREEN_BLK:
                        set_screen_blk();
                        break;
		case PKT_TYPE_SET_SCREEN_WHT:
		case PKT_TYPE_SET_SCREEN_RND_BW:
			set_screen_rnd_bw();
			break;
		case PKT_TYPE_SET_SCREEN_RND_COL:
			set_screen_rnd_col();
			break;

        case PKT_TYPE_RESET:
            send_reset();
            break;

		case PKT_TYPE_SET_SCREEN:
			if (p->hdr & PKT_MASK_RGB16)
			{
				if (p->pkt_len != 8 + 2 * 3 * ACAB_X * ACAB_Y)
				{
					LOG("PKTS: WARNING: dropping SET_SCREEN RGB16 pkt, invalid length");
					LOG(" %d != %d\n", p->pkt_len, 8 + 2 * 3 * ACAB_X * ACAB_Y);
					return;
				}
				set_screen_rgb16(p->hdr, (uint16_t (*)[ACAB_X][3])p->data);
			}
			if (p->hdr & PKT_MASK_RGB8)
			{
				if (p->pkt_len != 8 + 3 * ACAB_X * ACAB_Y)
				{
					LOG("PKTS: WARNING: dropping SET_SCREEN RGB8 pkt, invalid length");
					LOG(" %d != %d\n", p->pkt_len, 8 + 3 * ACAB_X * ACAB_Y);
					return;
				}
				uint64_t start, end;
				start = gettimeofday64();
				set_screen_rgb8(p->hdr, (uint8_t (*)[ACAB_X][3])p->data);
				end = gettimeofday64();
				LOG("PROF: set_screen = %lu\n", end - start);
			}

			break;

		case PKT_TYPE_SET_FRAME_RATE:
			if (p->pkt_len != 12)
			{
				LOG("PKTS: WARNING: dropping short SET_FRAME_RATE pkt\n");
				return;
			}
			frame_duration = 1000000 / (*((uint32_t *)p->data));
			next_frame();
			return;

		case PKT_TYPE_SET_DURATION:
			if (p->pkt_len != 12)
			{
				LOG("PKTS: WARNING: dropping short SET_DURATION pkt\n");
				return;
			}
			frame_duration = ntohl(*((uint32_t *)p->data));
                        LOG("PKTS: New duration %u\n", frame_duration);
			next_frame();
			return;

		/* out-of-band immediate commands follow */
		/* but were handled already async when entering gigargoyle, see in_packet() */
		case PKT_TYPE_FLUSH_FIFO:
		case PKT_TYPE_SHUTDOWN:
		default:
			return; /* drop unsupported packages */
	}

	/* FIXME: doesn't work
	if (p->hdr & PKT_MASK_DBL_BUF)
		flip_double_buffer();
	*/

	serve_web_clients();
}

void serve_web_clients(void)
{
	int ret;
	int i;

	struct timeval tv;
	tv.tv_sec  = 0;
	tv.tv_usec = 0;

	fd_set rfd;
	fd_set wfd;
	fd_set efd;

	for (i=0; i<MAX_WEB_CLIENTS; i++)
	{
		if (ggg->web->sock[i] != -1)
		{
			FD_ZERO(&rfd);
			FD_ZERO(&wfd);
			FD_ZERO(&efd);

			FD_SET(ggg->web->sock[i], &wfd);
			FD_SET(ggg->web->sock[i], &efd);

			select(ggg->web->sock[i]+1, &rfd, &wfd, &efd, &tv);

			if (FD_ISSET(ggg->web->sock[i], &efd))
			{
				close(ggg->web->sock[i]);
				ggg->web->sock[i] = -1;
				LOG("WEB: wizard%d disagrees no more\n", i);
			}

			if (FD_ISSET(ggg->web->sock[i], &wfd))
			{
				ret = write(ggg->web->sock[i], shadow_screen, ACAB_X*ACAB_Y*3);
				if (ret != ACAB_X*ACAB_Y*3)
				{
					LOG("PKTS: gigargoyle told wizard%d to move aside. she refused and is now dead.\n", i);
					close(ggg->web->sock[i]);
					ggg->web->sock[i] = -1;
				}
			}else{
				close(ggg->web->sock[i]);
				ggg->web->sock[i] = -1;
				LOG("WEB: wizard%d got some special treatment\n", i);
			}
		}
	}
}
