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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

#include "gigargoyle.h"
#include "packets.h"
#include "fifo.h"
#include "command_line_arguments.h"
#include "streamingsource.h"

void close_ss(streamingsource_t *ss)
{
	int ret;
	ret = close(ss->sock);
	if(ret)
	{
		LOG("ERROR: close(ss%d): %s\n",
				ss->type,
				strerror(errno));
	}

	// FIXME: beautify
	/*init_ss_l_socket(ss, ss->type == SOURCE_QM ?
			arguments.port_qm : arguments.port_is);*/
	if(ss->type == SOURCE_QM) {
		init_ss_l_socket(ss, arguments.port_qm);
	} else if (ss->type == SOURCE_IS) {
		ss->listener = -1;
	}

	ss->state = NET_NOT_CONNECTED;
	if ((ggg->source == SOURCE_QM && ss->type == SOURCE_QM) ||
		(ggg->source == SOURCE_IS && ggg->qm->state == NET_NOT_CONNECTED))
	{
		ggg->source = SOURCE_LOCAL;
		ggg->ss = NULL;
	}
	else if (ggg->source == SOURCE_IS && ss->type == SOURCE_IS &&
			ggg->qm->state == NET_CONNECTED)
	{
		ggg->source = SOURCE_QM;
		ggg->ss = ggg->qm;
	}
	else
	{
		LOG("SS: Unhandled condition while closing SS%d (source SS%d)\n",
			ss->type, ggg->source);
	}

	LOG("MAIN: listening for new SS%d connections\n", ss->type);
	ss->input_offset = 0;
}

void process_ss_l_data(streamingsource_t *ss)
{
	int ret;
	struct sockaddr_in ca;
	socklen_t salen = sizeof(struct sockaddr);

	ret = accept(ss->listener, (struct sockaddr *)&ca, &salen);
	if (ret < 0)
	{
		LOG("ERROR: accept() in  process_ss_l_data(): %s\n",
				strerror(errno));
		exit(1);
	}
	ss->sock = ret;
	ss->state = NET_CONNECTED;

	if (ggg->source != SOURCE_IS)
	{
		ggg->source = ss->type;
		ggg->ss = ss;
		flush_fifo();
	}

	LOG("MAIN: streaming source connected from %d.%d.%d.%d:%d\n",
			(ca.sin_addr.s_addr & 0x000000ff) >>  0,
			(ca.sin_addr.s_addr & 0x0000ff00) >>  8,
			(ca.sin_addr.s_addr & 0x00ff0000) >> 16,
			(ca.sin_addr.s_addr & 0xff000000) >> 24,
			ntohs(ca.sin_port)
	);

	if (close(ss->listener) < 0)
	{
		LOG("ERROR: close(ss->listener): %s\n", strerror(errno));
	}
}

void process_ss_data(streamingsource_t *ss)
{
	pkt_t p;
	pkt_t *pt;
	int ret;

	ret = read(ss->sock, ss->buf + ss->input_offset, BUF_SZ - ss->input_offset);
	if (ret == 0)
	{
		LOG("SS%d closed connection\n", ss->type);
		close_ss(ss);
		return;
	}
	if (ret < 0)
	{
		LOG("SS: WARNING: read(): %s\n",
		    strerror(errno));
		LOG("SS:          closing SS%d connection\n", ss->type);
		close_ss(ss);
		return;
	}

	pt = (pkt_t *) ss->buf;

	int plen = ret + ss->input_offset;
	int ret_pkt;
        do {
		p.hdr = ntohl(pt->hdr);
		p.pkt_len = ntohl(pt->pkt_len);
		p.data = (uint8_t *) &(pt->data);

		ret_pkt = check_packet(&p, plen);

		if(ret_pkt == -1) {
			/* too short */
			ss->input_offset += plen;
		} else {
			if(ret_pkt == 0) {
				if (ggg->source == SOURCE_QM) {
					/* queue packet */
					early_handle_packet(&p);
				} else if (ggg->source == SOURCE_IS) {
					/* process packet right now */
					handle_packet(&p);
				}
			}

			if( ((int)p.pkt_len <= plen) &&
			    ((int)p.pkt_len > 0)     &&
			    ((int)p.pkt_len < FIFO_WIDTH)) {
				plen -= (int)p.pkt_len;
				memmove(ss->buf, ss->buf + p.pkt_len, plen);
			}
			ss->input_offset = 0;
		}
	} while(ret_pkt == 0 && plen != 0);
}

void init_ss_l_socket(streamingsource_t *ss, uint16_t port)
{
	int ret;
	struct sockaddr_in sa;

	ss->listener = socket (AF_INET, SOCK_STREAM, 0);
	if (ss->listener < 0)
	{
		LOG("ERROR: socket() for streaming source: %s\n",
		    strerror(errno));
		exit(1);
	}
	memset(&sa, 0, sizeof(sa));
	sa.sin_family      = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port        = htons(port);

	ret = 1;
	if(setsockopt(ss->listener, SOL_SOCKET, SO_REUSEADDR,
				(char *)&ret, sizeof(ret)) < 0)
	{
		LOG("ERROR: setsockopt() for streaming source: %s\n",
		    strerror(errno));
		exit(1);
        }

	if (bind(ss->listener, (struct sockaddr *) &sa, sizeof(sa)) < 0)
	{
		LOG("MAIN: WARNING: bind() for streaming source failed. running without. no movie playing possible\n");
		ss->state = NET_ERROR;
		return;
	}

	if (listen(ss->listener, 8) < 0)
	{
		LOG("ERROR: listen() for streaming source: %s\n",
		    strerror(errno));
		exit(1);
	}
}


void init_streamingsource(streamingsource_t * ss)
{
	ss->buf = malloc(BUF_SZ);
	if (!ss->buf)
	{
		printf("ERROR: couldn't alloc %d buffer bytes: %s\n",
		       BUF_SZ,
		       strerror(errno));
		exit(1);
	}

	ss->input_offset = 0;
	ss->state = NET_NOT_CONNECTED;
}
