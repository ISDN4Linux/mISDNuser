/*
 *
 * Copyright 2014 Karsten Keil <kkeil@linux-pingi.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 *  Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 */

#ifndef _CALL_LOG_H
#define _CALL_LOG_H

struct mi_call_info {
	int		tei;
	int		cr;
	unsigned char 	calledPN;
	unsigned char	callingPN;
	unsigned char	connectedPN;
	int		cause;
	int		cause_loc;
	int		istate;
	int		state;
};

#endif /* _CALL_LOG_H */
